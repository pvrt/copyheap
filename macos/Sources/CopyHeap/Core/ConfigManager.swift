import Foundation
import Security

// MARK: - AppConfig

public struct AppConfig: Codable, Equatable {
    // githubToken is NOT persisted in JSON — stored in Keychain
    public var githubToken: String = ""
    public var gistId: String = ""
    public var syncEnabled: Bool = false
    public var lastSyncTime: Int64 = 0
    public var autostartEnabled: Bool = true

    enum CodingKeys: String, CodingKey {
        case gistId = "gist_id"
        case syncEnabled = "sync_enabled"
        case lastSyncTime = "last_sync_time"
        case autostartEnabled = "autostart_enabled"
    }

    public init(
        githubToken: String = "",
        gistId: String = "",
        syncEnabled: Bool = false,
        lastSyncTime: Int64 = 0,
        autostartEnabled: Bool = true
    ) {
        self.githubToken = githubToken
        self.gistId = gistId
        self.syncEnabled = syncEnabled
        self.lastSyncTime = lastSyncTime
        self.autostartEnabled = autostartEnabled
    }
}

// MARK: - ConfigManager

public class ConfigManager {
    public private(set) var config: AppConfig = AppConfig()
    private let configURL: URL
    private let lock = NSLock()

    // MARK: Keychain constants
    private static let keychainService = "com.copyheap.macos"
    private static let keychainAccount = "github_token"

    public init(customURL: URL? = nil) {
        if let customURL = customURL {
            self.configURL = customURL
        } else {
            let appSupport = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
            let dir = appSupport.appendingPathComponent("CopyHeap", isDirectory: true)
            try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
            self.configURL = dir.appendingPathComponent("config.json")
        }
        load()
    }

    @discardableResult
    public func load() -> Bool {
        lock.lock()
        defer { lock.unlock() }
        if FileManager.default.fileExists(atPath: configURL.path),
           let data = try? Data(contentsOf: configURL),
           let decoded = try? JSONDecoder().decode(AppConfig.self, from: data) {
            self.config = decoded
        }
        // Always inject token from Keychain (even if JSON decode failed)
        self.config.githubToken = ConfigManager.loadTokenFromKeychain() ?? ""
        return true
    }

    @discardableResult
    public func save() -> Bool {
        lock.lock()
        defer { lock.unlock() }
        // Persist token to Keychain, not JSON
        ConfigManager.saveTokenToKeychain(config.githubToken)
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted]
        guard let data = try? encoder.encode(config) else { return false }
        do {
            try data.write(to: configURL, options: .atomic)
            return true
        } catch {
            return false
        }
    }

    public func updateConfig(_ mutate: (inout AppConfig) -> Void) {
        lock.lock()
        mutate(&config)
        lock.unlock()
        save()
    }

    // MARK: - Keychain helpers

    private static func saveTokenToKeychain(_ token: String) {
        guard let data = token.data(using: .utf8) else { return }
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: keychainService,
            kSecAttrAccount as String: keychainAccount,
            kSecValueData as String: data
        ]
        SecItemDelete(query as CFDictionary)
        SecItemAdd(query as CFDictionary, nil)
    }

    private static func loadTokenFromKeychain() -> String? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: keychainService,
            kSecAttrAccount as String: keychainAccount,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne
        ]
        var result: AnyObject?
        guard SecItemCopyMatching(query as CFDictionary, &result) == errSecSuccess,
              let data = result as? Data,
              let token = String(data: data, encoding: .utf8) else { return nil }
        return token
    }
}
