import Foundation

public struct ClipboardItem: Identifiable, Codable, Equatable {
    public let id: String
    public var title: String
    public var content: String
    public var contentHash: UInt64
    public var isPinned: Bool
    public var createdAt: Int64
    public var pinnedAt: Int64

    enum CodingKeys: String, CodingKey {
        case id
        case title
        case content
        case contentHash = "content_hash"
        case isPinned = "is_pinned"
        case createdAt = "created_at"
        case pinnedAt = "pinned_at"
    }

    public init(
        id: String = UUID().uuidString,
        title: String = "",
        content: String,
        contentHash: UInt64 = 0,
        isPinned: Bool = false,
        createdAt: Int64 = Int64(Date().timeIntervalSince1970 * 1000),
        pinnedAt: Int64 = 0
    ) {
        self.id = id
        self.title = title
        self.content = content
        self.contentHash = contentHash != 0 ? contentHash : FNVHash.fnv1a64(content)
        self.isPinned = isPinned
        self.createdAt = createdAt
        self.pinnedAt = pinnedAt
    }

    public var displayTitle: String {
        let trimmedTitle = title.trimmingCharacters(in: .whitespacesAndNewlines)
        if !trimmedTitle.isEmpty {
            return trimmedTitle
        }
        for line in content.components(separatedBy: .newlines) {
            let trimmed = line.trimmingCharacters(in: .whitespacesAndNewlines)
            if !trimmed.isEmpty {
                return trimmed
            }
        }
        return "Без названия"
    }
}

public enum PinResult {
    case success
    case notFound
    case limitReached
}
