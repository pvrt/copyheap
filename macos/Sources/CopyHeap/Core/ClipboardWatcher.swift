import AppKit

public final class ClipboardWatcher {
    public typealias ChangeHandler = (String) -> Void

    private var timer: Timer?
    private var lastChangeCount: Int
    private var lastTextHash: UInt64 = 0
    private let onNewClip: ChangeHandler

    public init(onNewClip: @escaping ChangeHandler) {
        self.onNewClip = onNewClip
        self.lastChangeCount = NSPasteboard.general.changeCount
    }

    public func start() {
        stop()
        lastChangeCount = NSPasteboard.general.changeCount
        timer = Timer.scheduledTimer(withTimeInterval: 0.35, repeats: true) { [weak self] _ in
            self?.checkPasteboard()
        }
        if let timer = timer {
            RunLoop.main.add(timer, forMode: .common)
        }
    }

    public func stop() {
        timer?.invalidate()
        timer = nil
    }

    public func copyToClipboard(text: String) {
        let pb = NSPasteboard.general
        pb.clearContents()
        pb.setString(text, forType: .string)
        lastChangeCount = pb.changeCount
        lastTextHash = FNVHash.fnv1a64(text)
    }

    private func checkPasteboard() {
        let pb = NSPasteboard.general
        let currentCount = pb.changeCount
        guard currentCount != lastChangeCount else { return }
        lastChangeCount = currentCount

        // Filter out transient or password-protected types (e.g. 1Password, Bitwarden)
        let types = pb.types ?? []
        for type in types {
            let identifier = type.rawValue
            if identifier.contains("TransientType") ||
               identifier.contains("ConcealedType") ||
               identifier.contains("onepassword") {
                return
            }
        }

        guard let text = pb.string(forType: .string) else { return }
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }

        let hash = FNVHash.fnv1a64(text)
        if hash == lastTextHash { return }
        lastTextHash = hash

        onNewClip(text)
    }
}
