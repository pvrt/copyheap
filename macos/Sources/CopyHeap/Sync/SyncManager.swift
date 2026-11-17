import Foundation
import Network

public final class SyncManager {
    private let storage: StorageManager
    private let configManager: ConfigManager

    private var syncDebounceTask: Task<Void, Never>?
    private var periodicTimer: Timer?
    private var isSyncInProgress = false
    public var onSyncCompleted: (() -> Void)?

    public init(storage: StorageManager, configManager: ConfigManager) {
        self.storage = storage
        self.configManager = configManager
        startPeriodicSync()
    }

    deinit {
        periodicTimer?.invalidate()
        syncDebounceTask?.cancel()
    }

    public func startPeriodicSync() {
        periodicTimer?.invalidate()
        // Check every 15 minutes (900 seconds)
        periodicTimer = Timer.scheduledTimer(withTimeInterval: 900, repeats: true) { [weak self] _ in
            self?.triggerBackgroundSync()
        }
    }

    /// Triggers debounced sync (e.g. 5 seconds after pin/unpin/edit/delete)
    public func scheduleDebouncedSync(delaySeconds: Double = 5.0) {
        syncDebounceTask?.cancel()
        syncDebounceTask = Task { [weak self] in
            try? await Task.sleep(nanoseconds: UInt64(delaySeconds * 1_000_000_000))
            if !Task.isCancelled {
                self?.triggerBackgroundSync()
            }
        }
    }

    /// Silent background sync that ignores errors if offline or token missing
    public func triggerBackgroundSync() {
        guard !isSyncInProgress else { return }
        let token = configManager.config.githubToken.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !token.isEmpty else { return }

        Task {
            do {
                self.isSyncInProgress = true
                defer { self.isSyncInProgress = false }
                try await self.syncNow()
                await MainActor.run { [weak self] in
                    self?.onSyncCompleted?()
                }
            } catch {
                // Background sync silently fails on network / temporary issues
            }
        }
    }

    public enum SyncError: LocalizedError {
        case missingToken
        case encryptionFailed
        case decryptionFailed
        case networkError(String)
        case noNetwork

        public var errorDescription: String? {
            switch self {
            case .missingToken:
                return "GitHub Personal Access Token не указан."
            case .encryptionFailed:
                return "Ошибка шифрования AES-256."
            case .decryptionFailed:
                return "Ошибка расшифровки данных из Gist (неверный токен?)."
            case .networkError(let msg):
                return "Ошибка сети: \(msg)"
            case .noNetwork:
                return "Нет подключения к интернету. Проверьте соединение."
            }
        }
    }

    private func isNetworkAvailable() async -> Bool {
        return await withCheckedContinuation { continuation in
            let monitor = NWPathMonitor()
            let queue = DispatchQueue(label: "com.copyheap.network-check")
            monitor.pathUpdateHandler = { path in
                monitor.cancel()
                continuation.resume(returning: path.status == .satisfied)
            }
            monitor.start(queue: queue)
        }
    }

    public func syncNow() async throws {
        guard await isNetworkAvailable() else {
            throw SyncError.networkError("Нет подключения к интернету")
        }

        let config = configManager.config
        let token = config.githubToken.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !token.isEmpty else {
            throw SyncError.missingToken
        }

        // 1. Fetch or create Gist
        let (gistId, rawContent) = try await GistClient.findOrCreateGist(
            token: token,
            existingGistId: config.gistId.isEmpty ? nil : config.gistId
        )

        // Update stored gist ID if newly created
        if config.gistId != gistId {
            configManager.updateConfig { $0.gistId = gistId }
        }

        // 2. If existing content exists in Gist, decrypt & merge
        if let rawContent = rawContent, !rawContent.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            let jsonString: String
            if rawContent.contains("\"encrypted\":") && rawContent.contains("true") {
                guard let decrypted = CryptoUtils.decryptAES256(payloadJSON: rawContent, keyMaterial: token) else {
                    throw SyncError.decryptionFailed
                }
                jsonString = decrypted
            } else {
                jsonString = rawContent
            }

            if let data = jsonString.data(using: .utf8),
               let remoteItems = try? JSONDecoder().decode([ClipboardItem].self, from: data) {
                storage.mergePinnedFromRemote(remoteItems)
            }
        }

        // 3. Encrypt local pinned items and upload to Gist
        let pinnedItems = storage.getPinnedItems()
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted]
        guard let localData = try? encoder.encode(pinnedItems),
              let localJson = String(data: localData, encoding: .utf8) else {
            throw SyncError.encryptionFailed
        }

        guard let encryptedPayload = CryptoUtils.encryptAES256(plaintext: localJson, keyMaterial: token) else {
            throw SyncError.encryptionFailed
        }

        try await GistClient.updateGist(token: token, gistId: gistId, content: encryptedPayload)

        // 4. Update sync timestamp
        let now = Int64(Date().timeIntervalSince1970 * 1000)
        configManager.updateConfig {
            $0.lastSyncTime = now
            $0.syncEnabled = true
        }
    }
}
