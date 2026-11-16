import AppKit
import SwiftUI

@MainActor
public final class MenuBarManager: NSObject, NSMenuDelegate {
    private var statusItem: NSStatusItem!
    private let appState: AppState
    private var mainWindowController: NSWindowController?
    private var syncWindowController: NSWindowController?

    public init(appState: AppState) {
        self.appState = appState
        super.init()
        setupStatusItem()
    }

    private func setupStatusItem() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        if let button = statusItem.button {
            button.image = createStatusBarImage()
            button.imagePosition = .imageOnly
        }
        let menu = NSMenu()
        menu.delegate = self
        statusItem.menu = menu
    }

    private func createStatusBarImage() -> NSImage {
        // If bundled icon exists in Resources, prefer it
        if let bundleImage = Bundle.main.image(forResource: "StatusBarIcon") {
            bundleImage.isTemplate = true
            return bundleImage
        }

        // Vector drawing of CopyHeap horseshoe icon (18x18 pt template image)
        let img = NSImage(size: NSSize(width: 18, height: 18), flipped: false) { _ in
            guard let ctx = NSGraphicsContext.current?.cgContext else { return false }
            ctx.setFillColor(NSColor.black.cgColor)
            let center = CGPoint(x: 9.0, y: 8.5)
            let outerR: CGFloat = 7.2
            let innerR: CGFloat = 4.0

            let path = CGMutablePath()
            let startAngle: CGFloat = 62.0 * .pi / 180.0
            let endAngle: CGFloat = 118.0 * .pi / 180.0

            path.addArc(center: center, radius: outerR, startAngle: startAngle, endAngle: endAngle, clockwise: false)
            let pLeftInner = CGPoint(x: center.x + innerR * cos(endAngle), y: center.y + innerR * sin(endAngle))
            path.addLine(to: pLeftInner)
            path.addArc(center: center, radius: innerR, startAngle: endAngle, endAngle: startAngle, clockwise: true)
            path.closeSubpath()

            ctx.addPath(path)
            ctx.fillPath()
            return true
        }
        img.isTemplate = true
        return img
    }

    public func menuNeedsUpdate(_ menu: NSMenu) {
        menu.removeAllItems()
        populateMenu(menu)
    }

    private func populateMenu(_ menu: NSMenu) {
        // 1. "Открыть CopyHeap..."
        let openItem = NSMenuItem(title: "Открыть CopyHeap...", action: #selector(openMainWindow), keyEquivalent: "o")
        openItem.target = self
        menu.addItem(openItem)

        // 2. "Синхронизация..."
        let syncItem = NSMenuItem(title: "Синхронизация...", action: #selector(openSyncWindow), keyEquivalent: "s")
        syncItem.target = self
        menu.addItem(syncItem)

        menu.addItem(NSMenuItem.separator())

        // 3. Pinned snippets
        let pinned = appState.storage.getPinnedItems()
        if !pinned.isEmpty {
            let header = NSMenuItem(title: "Закреплённые сниппеты:", action: nil, keyEquivalent: "")
            header.isEnabled = false
            menu.addItem(header)

            for item in pinned.prefix(25) {
                let title = "•  " + truncate(item.displayTitle, length: 30)
                let menuItem = NSMenuItem(title: title, action: #selector(clipboardMenuItemClicked(_:)), keyEquivalent: "")
                menuItem.target = self
                menuItem.representedObject = item
                menu.addItem(menuItem)
            }
            menu.addItem(NSMenuItem.separator())
        }

        // 4. Recent history
        let recent = appState.storage.getUnpinnedItems()
        let headerRecent = NSMenuItem(title: "История буфера обмена:", action: nil, keyEquivalent: "")
        headerRecent.isEnabled = false
        menu.addItem(headerRecent)

        if recent.isEmpty {
            let emptyItem = NSMenuItem(title: "История пуста", action: nil, keyEquivalent: "")
            emptyItem.isEnabled = false
            menu.addItem(emptyItem)
        } else {
            for item in recent.prefix(35) {
                let title = "   " + truncate(item.displayTitle, length: 30)
                let menuItem = NSMenuItem(title: title, action: #selector(clipboardMenuItemClicked(_:)), keyEquivalent: "")
                menuItem.target = self
                menuItem.representedObject = item
                menu.addItem(menuItem)
            }
        }

        menu.addItem(NSMenuItem.separator())

        // 5. "Очистить историю"
        let clearItem = NSMenuItem(title: "Очистить историю", action: #selector(clearHistoryClicked), keyEquivalent: "")
        clearItem.target = self
        menu.addItem(clearItem)

        // 6. "Завершить CopyHeap"
        let quitItem = NSMenuItem(title: "Завершить CopyHeap", action: #selector(quitApp), keyEquivalent: "q")
        quitItem.target = self
        menu.addItem(quitItem)
    }

    private func truncate(_ str: String, length: Int) -> String {
        let singleLine = str.replacingOccurrences(of: "\n", with: " ").trimmingCharacters(in: .whitespaces)
        if singleLine.count <= length { return singleLine }
        return String(singleLine.prefix(length)) + "..."
    }

    @objc private func clipboardMenuItemClicked(_ sender: NSMenuItem) {
        guard let item = sender.representedObject as? ClipboardItem else { return }
        appState.copyItem(item)
    }

    @objc public func openMainWindow() {
        if let controller = mainWindowController, let window = controller.window {
            window.makeKeyAndOrderFront(nil)
            NSApp.activate(ignoringOtherApps: true)
            return
        }

        let contentView = MainWindowView(appState: appState)
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 800, height: 500),
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.center()
        window.title = "CopyHeap"
        window.contentView = NSHostingView(rootView: contentView)
        window.isReleasedWhenClosed = false

        let controller = NSWindowController(window: window)
        mainWindowController = controller
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    @objc public func openSyncWindow() {
        if let controller = syncWindowController, let window = controller.window {
            window.makeKeyAndOrderFront(nil)
            NSApp.activate(ignoringOtherApps: true)
            return
        }

        let contentView = SyncSettingsView(appState: appState)
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 480, height: 320),
            styleMask: [.titled, .closable],
            backing: .buffered,
            defer: false
        )
        window.center()
        window.title = "Синхронизация CopyHeap"
        window.contentView = NSHostingView(rootView: contentView)
        window.isReleasedWhenClosed = false

        let controller = NSWindowController(window: window)
        syncWindowController = controller
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    @objc private func clearHistoryClicked() {
        appState.storage.clearHistory(keepPinned: true)
        appState.refresh()
    }

    @objc private func quitApp() {
        appState.storage.flush()
        NSApplication.shared.terminate(nil)
    }
}
