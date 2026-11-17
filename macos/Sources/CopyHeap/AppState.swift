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

        self.syncManager.onSyncCompleted = { [weak self] in
            Task { @MainActor in
                self?.refresh()
            }
        }

        // Silent background sync on launch
        self.syncManager.triggerBackgroundSync()

        self.clipboardWatcher = ClipboardWatcher { [weak self] text in
            Task { @MainActor in
                guard let self = self else { return }
                let newItem = self.storage.addOrUpdate(content: text)
                // Check if item is truly new (not just a bump of existing)
                if self.items.first?.id != newItem.id {
                    self.itemAdded(newItem)
                } else {
                    self.items[0] = newItem // update timestamp in place
                }
            }
        }
        self.clipboardWatcher.start()
    }

    public func refresh() {
        self.items = storage.getAllItems()
        self.config = configManager.config
    }

    public func itemAdded(_ item: ClipboardItem) {
        // Insert at top, prune if needed — no full reload from storage
        items.insert(item, at: 0)
        // Keep only unpinned up to max
        var unpinnedCount = items.filter { !$0.isPinned }.count
        items.removeAll(where: { item in
            if item.isPinned { return false }
            unpinnedCount -= 1
            return unpinnedCount >= StorageManager.maxUnpinnedItems
        })
    }

    public func copyItem(_ item: ClipboardItem) {
        clipboardWatcher.copyToClipboard(text: item.content)
    }
}
