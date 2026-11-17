import AppKit

public final class ClipboardWatcher {
    public typealias ChangeHandler = (String) -> Void

    private var timer: Timer?
    private var lastChangeCount: Int
    private var lastTextHash: UInt64 = 0
    private let onNewClip: ChangeHandler

    // Adaptive polling
    private var idleCount = 0
    private let fastInterval: TimeInterval = 0.35
    private let slowInterval: TimeInterval = 1.0

    public init(onNewClip: @escaping ChangeHandler) {
        self.onNewClip = onNewClip
        self.lastChangeCount = NSPasteboard.general.changeCount
    }

    public func start() {
        stop()
        idleCount = 0
        lastChangeCount = NSPasteboard.general.changeCount
        timer = Timer.scheduledTimer(withTimeInterval: fastInterval, repeats: true) { [weak self] _ in
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
        guard currentCount != lastChangeCount else {
            idleCount += 1
            // After 10 consecutive no-change checks (~3.5s), slow down to 1.0s interval
            if idleCount == 10 {
                timer?.invalidate()
                timer = Timer.scheduledTimer(withTimeInterval: slowInterval, repeats: true) { [weak self] _ in
                    self?.checkPasteboard()
                }
                RunLoop.main.add(timer!, forMode: .common)
            }
            return
        }

        // Reset to fast mode if we were in slow mode
        if idleCount >= 10 {
            timer?.invalidate()
            timer = Timer.scheduledTimer(withTimeInterval: fastInterval, repeats: true) { [weak self] _ in
                self?.checkPasteboard()
            }
            RunLoop.main.add(timer!, forMode: .common)
        }
        idleCount = 0
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
