import Foundation

public struct AppConfig: Codable, Equatable {
    public var githubToken: String = ""
    public var gistId: String = ""
    public var syncEnabled: Bool = false
    public var lastSyncTime: Int64 = 0
    public var autostartEnabled: Bool = true

    enum CodingKeys: String, CodingKey {
        case githubToken = "github_token"
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

public class ConfigManager {
    public private(set) var config: AppConfig = AppConfig()
    private let configURL: URL
    private let lock = NSLock()

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
        guard FileManager.default.fileExists(atPath: configURL.path),
              let data = try? Data(contentsOf: configURL),
              let decoded = try? JSONDecoder().decode(AppConfig.self, from: data) else {
            return false
        }
        self.config = decoded
        return true
    }

    @discardableResult
    public func save() -> Bool {
        lock.lock()
        defer { lock.unlock() }
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
}
