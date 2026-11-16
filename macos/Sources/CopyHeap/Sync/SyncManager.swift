import Foundation

public final class SyncManager {
    private let storage: StorageManager
    private let configManager: ConfigManager

    public init(storage: StorageManager, configManager: ConfigManager) {
        self.storage = storage
        self.configManager = configManager
    }

    public enum SyncError: LocalizedError {
        case missingToken
        case encryptionFailed
        case decryptionFailed
        case networkError(String)

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
            }
        }
    }

    public func syncNow() async throws {
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
