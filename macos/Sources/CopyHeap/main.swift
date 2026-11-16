import AppKit
import SwiftUI

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var appState: AppState!
    private var menuBarManager: MenuBarManager!

    func applicationDidFinishLaunching(_ notification: Notification) {
        self.appState = AppState()
        self.menuBarManager = MenuBarManager(appState: appState)
    }

    func applicationWillTerminate(_ notification: Notification) {
        appState?.storage.flush()
    }
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.accessory) // Runs as menu bar item without Dock icon
app.run()
