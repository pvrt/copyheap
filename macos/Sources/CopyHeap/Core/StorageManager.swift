import Foundation

public final class StorageManager: @unchecked Sendable {
    public static let maxUnpinnedItems = 200

    private var items: [ClipboardItem] = []
    private let storageURL: URL
    private let lock = NSLock()
    private var saveWorkItem: DispatchWorkItem?
    private let saveQueue = DispatchQueue(label: "com.copyheap.storage", qos: .utility)

    public init(customURL: URL? = nil) {
        if let customURL = customURL {
            self.storageURL = customURL
        } else {
            let appSupport = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
            let dir = appSupport.appendingPathComponent("CopyHeap", isDirectory: true)
            try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
            self.storageURL = dir.appendingPathComponent("history.json")
        }
        loadFromFile()
    }

    public func getAllItems() -> [ClipboardItem] {
        lock.lock()
        defer { lock.unlock() }
        return items
    }

    public func getPinnedItems() -> [ClipboardItem] {
        lock.lock()
        defer { lock.unlock() }
        return items.filter { $0.isPinned }
    }

    public func getUnpinnedItems() -> [ClipboardItem] {
        lock.lock()
        defer { lock.unlock() }
        return items.filter { !$0.isPinned }
    }

    @discardableResult
    public func addOrUpdate(content: String, title: String = "") -> ClipboardItem {
        lock.lock()
        defer { lock.unlock() }

        let hash = FNVHash.fnv1a64(content)
        let now = Int64(Date().timeIntervalSince1970 * 1000)

        if let existingIdx = items.firstIndex(where: { $0.contentHash == hash && $0.content == content }) {
            var item = items.remove(at: existingIdx)
            item.createdAt = now
            if !title.isEmpty { item.title = title }
            items.insert(item, at: 0)
            scheduleSave()
            return item
        }

        let newItem = ClipboardItem(
            id: UUID().uuidString,
            title: title,
            content: content,
            contentHash: hash,
            isPinned: false,
            createdAt: now,
            pinnedAt: 0
        )
        items.insert(newItem, at: 0)
        pruneUnpinned()
        scheduleSave()
        return newItem
    }

    @discardableResult
    public func togglePin(id: String) -> PinResult {
        lock.lock()
        defer { lock.unlock() }

        guard let idx = items.firstIndex(where: { $0.id == id }) else {
            return .notFound
        }

        var item = items[idx]
        item.isPinned.toggle()
        item.pinnedAt = item.isPinned ? Int64(Date().timeIntervalSince1970 * 1000) : 0
        items[idx] = item

        pruneUnpinned()
        scheduleSave()
        return .success
    }

    @discardableResult
    public func updateItem(id: String, title: String, content: String) -> Bool {
        lock.lock()
        defer { lock.unlock() }

        guard let idx = items.firstIndex(where: { $0.id == id }) else {
            return false
        }

        items[idx].title = title
        items[idx].content = content
        items[idx].contentHash = FNVHash.fnv1a64(content)
        scheduleSave()
        return true
    }

    @discardableResult
    public func deleteItem(id: String) -> Bool {
        lock.lock()
        defer { lock.unlock() }

        guard let idx = items.firstIndex(where: { $0.id == id }) else {
            return false
        }
        items.remove(at: idx)
        scheduleSave()
        return true
    }

    public func clearHistory(keepPinned: Bool = true) {
        lock.lock()
        defer { lock.unlock() }

        if keepPinned {
            items.removeAll { !$0.isPinned }
        } else {
            items.removeAll()
        }
        scheduleSave()
    }

    public func search(query: String) -> [ClipboardItem] {
        lock.lock()
        defer { lock.unlock() }

        let trimmed = query.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        if trimmed.isEmpty { return items }

        return items.filter {
            $0.title.lowercased().contains(trimmed) ||
            $0.content.lowercased().contains(trimmed)
        }
    }

    public func mergePinnedFromRemote(_ remoteItems: [ClipboardItem]) {
        lock.lock()
        defer { lock.unlock() }

        for remote in remoteItems {
            if let existingIdx = items.firstIndex(where: { $0.contentHash == remote.contentHash && $0.content == remote.content }) {
                items[existingIdx].isPinned = true
                if items[existingIdx].title.isEmpty && !remote.title.isEmpty {
                    items[existingIdx].title = remote.title
                }
            } else {
                var newPinned = remote
                newPinned.isPinned = true
                items.insert(newPinned, at: 0)
            }
        }
        pruneUnpinned()
        scheduleSave()
    }

    private func pruneUnpinned() {
        var unpinnedCount = 0
        items.removeAll { item in
            if item.isPinned {
                return false
            }
            unpinnedCount += 1
            return unpinnedCount > Self.maxUnpinnedItems
        }
    }

    private func scheduleSave() {
        saveWorkItem?.cancel()
        let work = DispatchWorkItem { [weak self] in
            self?.saveToFile()
        }
        saveWorkItem = work
        saveQueue.asyncAfter(deadline: .now() + 0.3, execute: work)
    }

    public func flush() {
        lock.lock()
        defer { lock.unlock() }
        saveWorkItem?.cancel()
        saveToFile()
    }

    private func saveToFile() {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted]
        guard let data = try? encoder.encode(items) else { return }
        try? data.write(to: storageURL, options: .atomic)
    }

    @discardableResult
    public func loadFromFile() -> Bool {
        lock.lock()
        defer { lock.unlock() }

        guard FileManager.default.fileExists(atPath: storageURL.path),
              let data = try? Data(contentsOf: storageURL),
              let decoded = try? JSONDecoder().decode([ClipboardItem].self, from: data) else {
            return false
        }
        self.items = decoded
        pruneUnpinned()
        return true
    }
}
