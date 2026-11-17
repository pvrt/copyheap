import SwiftUI
import AppKit

public struct MainWindowView: View {
    @ObservedObject var appState: AppState

    @State private var searchQuery: String = ""
    @State private var debouncedQuery: String = ""
    @State private var searchDebounceTask: Task<Void, Never>?
    @State private var selectedItemId: String?
    @State private var isShowingSyncSettings: Bool = false
    @State private var editingItem: ClipboardItem?
    @State private var isShowingEditor: Bool = false

    private var filteredItems: [ClipboardItem] {
        if debouncedQuery.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            return appState.items
        }
        return appState.storage.search(query: debouncedQuery)
    }

    private var selectedItem: ClipboardItem? {
        appState.items.first(where: { $0.id == selectedItemId })
    }

    public var body: some View {
        HSplitView {
            // Left sidebar: list of items
            VStack(spacing: 0) {
                // Search bar and action buttons
                HStack(spacing: 8) {
                    Image(systemName: "magnifyingglass")
                        .foregroundColor(.secondary)
                    TextField("Поиск (Ctrl/Cmd+F)...", text: $searchQuery)
                        .textFieldStyle(.plain)

                    if !searchQuery.isEmpty {
                        Button(action: { searchQuery = "" }) {
                            Image(systemName: "xmark.circle.fill")
                                .foregroundColor(.secondary)
                        }
                        .buttonStyle(.plain)
                    }

                    Button(action: {
                        editingItem = nil
                        isShowingEditor = true
                    }) {
                        Image(systemName: "plus")
                    }
                    .help("Добавить сниппет вручную")
                }
                .padding(10)
                .background(Color(NSColor.controlBackgroundColor))

                Divider()

                // List
                List(selection: $selectedItemId) {
                    let pinned = filteredItems.filter { $0.isPinned }
                    let recent = filteredItems.filter { !$0.isPinned }

                    if !pinned.isEmpty {
                        Section("Закреплённые") {
                            ForEach(pinned) { item in
                                ItemRowView(item: item, isSelected: selectedItemId == item.id)
                                    .tag(item.id)
                                    .contextMenu {
                                        contextMenuItems(for: item)
                                    }
                            }
                        }
                    }

                    if !recent.isEmpty {
                        Section("История") {
                            ForEach(recent) { item in
                                ItemRowView(item: item, isSelected: selectedItemId == item.id)
                                    .tag(item.id)
                                    .contextMenu {
                                        contextMenuItems(for: item)
                                    }
                            }
                        }
                    }

                    if filteredItems.isEmpty {
                        Text("Ничего не найдено")
                            .foregroundColor(.secondary)
                            .font(.callout)
                            .padding()
                    }
                }
                .listStyle(.sidebar)

                Divider()

                // Bottom bar
                HStack {
                    Text("\(filteredItems.count) записей")
                        .font(.caption2)
                        .foregroundColor(.secondary)

                    Spacer()

                    Button(action: { isShowingSyncSettings = true }) {
                        Image(systemName: "arrow.triangle.2.circlepath")
                    }
                    .help("Синхронизация с GitHub Gist")

                    Button(action: {
                        appState.storage.clearHistory(keepPinned: true)
                        appState.refresh()
                    }) {
                        Image(systemName: "trash")
                    }
                    .help("Очистить историю (кроме закреплённых)")
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 6)
                .background(Color(NSColor.controlBackgroundColor))
            }
            .frame(minWidth: 260, idealWidth: 300, maxWidth: 450)

            // Right pane: content viewer / detail
            VStack(alignment: .leading, spacing: 0) {
                if let item = selectedItem {
                    HStack {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(item.displayTitle)
                                .font(.headline)
                                .lineLimit(1)

                            let date = Date(timeIntervalSince1970: Double(item.createdAt) / 1000.0)
                            Text("Скопировано: \(date.formatted(date: .abbreviated, time: .standard)) • \(item.content.count) симв.")
                                .font(.caption2)
                                .foregroundColor(.secondary)
                        }

                        Spacer()

                        Button(action: {
                            appState.copyItem(item)
                        }) {
                            Label("Скопировать", systemImage: "doc.on.doc")
                        }

                        Button(action: {
                            appState.storage.togglePin(id: item.id)
                            appState.refresh()
                            appState.syncManager.scheduleDebouncedSync()
                        }) {
                            Image(systemName: item.isPinned ? "pin.fill" : "pin")
                                .foregroundColor(item.isPinned ? .accentColor : .primary)
                        }
                        .help(item.isPinned ? "Открепить" : "Закрепить")

                        Button(action: {
                            editingItem = item
                            isShowingEditor = true
                        }) {
                            Image(systemName: "pencil")
                        }
                        .help("Редактировать")
                    }
                    .padding(12)
                    .background(Color(NSColor.windowBackgroundColor))

                    Divider()

                    ScrollView {
                        Text(item.content)
                            .font(.system(.body, design: .monospaced))
                            .textSelection(.enabled)
                            .padding(12)
                            .frame(maxWidth: .infinity, alignment: .leading)
                    }
                    .background(Color(NSColor.textBackgroundColor))
                } else {
                    VStack(spacing: 8) {
                        Image(systemName: "doc.on.clipboard")
                            .font(.system(size: 40))
                            .foregroundColor(.secondary.opacity(0.6))
                        Text("Выберите запись для просмотра")
                            .font(.callout)
                            .foregroundColor(.secondary)
                    }
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                }
            }
            .frame(minWidth: 320, idealWidth: 460)
        }
        .frame(minWidth: 640, minHeight: 440)
        .sheet(isPresented: $isShowingSyncSettings) {
            SyncSettingsView(appState: appState)
        }
        .sheet(isPresented: $isShowingEditor) {
            SnippetEditorView(appState: appState, editingItem: editingItem)
        }
        .onAppear {
            if selectedItemId == nil, let first = appState.items.first {
                selectedItemId = first.id
            }
        }
        .onChange(of: searchQuery) { newValue in
            searchDebounceTask?.cancel()
            searchDebounceTask = Task {
                try? await Task.sleep(nanoseconds: 150_000_000)
                if !Task.isCancelled {
                    debouncedQuery = newValue
                }
            }
        }
    }

    @ViewBuilder
    private func contextMenuItems(for item: ClipboardItem) -> some View {
        Button("Скопировать") {
            appState.copyItem(item)
        }
        Button(item.isPinned ? "Открепить" : "Закрепить") {
            appState.storage.togglePin(id: item.id)
            appState.refresh()
        }
        Button("Редактировать...") {
            editingItem = item
            isShowingEditor = true
        }
        Divider()
        Button("Удалить", role: .destructive) {
            appState.storage.deleteItem(id: item.id)
            appState.refresh()
            if selectedItemId == item.id {
                selectedItemId = nil
            }
        }
    }
}

private struct ItemRowView: View {
    let item: ClipboardItem
    let isSelected: Bool

    var body: some View {
        HStack(spacing: 8) {
            if item.isPinned {
                Circle()
                    .fill(Color.accentColor)
                    .frame(width: 7, height: 7)
            } else {
                Spacer().frame(width: 7)
            }

            VStack(alignment: .leading, spacing: 2) {
                Text(item.displayTitle)
                    .font(.body)
                    .lineLimit(1)

                Text("\(previewSnippet(item.content)) • \(item.content.count) симв.")
                    .font(.caption2)
                    .foregroundColor(.secondary)
                    .lineLimit(1)
            }

            Spacer()
        }
        .padding(.vertical, 3)
    }

    private func previewSnippet(_ text: String) -> String {
        let oneLine = text.replacingOccurrences(of: "\n", with: " ").trimmingCharacters(in: .whitespaces)
        return String(oneLine.prefix(60))
    }
}
