import Foundation
import Combine
import AppKit

@MainActor
public final class AppState: ObservableObject {
    @Published public var items: [ClipboardItem] = []
    @Published public var config: AppConfig

    public let storage: StorageManager
    public let configManager: ConfigManager
    public let syncManager: SyncManager
    public private(set) var clipboardWatcher: ClipboardWatcher!

    public init(storage: StorageManager = StorageManager(), configManager: ConfigManager = ConfigManager()) {
        self.storage = storage
        self.configManager = configManager
        self.config = configManager.config
        self.syncManager = SyncManager(storage: storage, configManager: configManager)
        self.items = storage.getAllItems()

        self.clipboardWatcher = ClipboardWatcher { [weak self] text in
            Task { @MainActor in
                self?.storage.addOrUpdate(content: text)
                self?.refresh()
            }
        }
        self.clipboardWatcher.start()
    }

    public func refresh() {
        self.items = storage.getAllItems()
        self.config = configManager.config
    }

    public func copyItem(_ item: ClipboardItem) {
        clipboardWatcher.copyToClipboard(text: item.content)
    }
}
