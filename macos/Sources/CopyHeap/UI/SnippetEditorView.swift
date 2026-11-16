import SwiftUI

public struct SnippetEditorView: View {
    @ObservedObject var appState: AppState
    @Environment(\.dismiss) private var dismiss

    var editingItem: ClipboardItem?

    @State private var title: String = ""
    @State private var content: String = ""

    public var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text(editingItem == nil ? "Новый сниппет" : "Редактирование сниппета")
                .font(.headline)

            VStack(alignment: .leading, spacing: 4) {
                Text("Название (опционально):")
                    .font(.caption)
                    .foregroundColor(.secondary)
                TextField("Например: SQL шаблон или адрес", text: $title)
                    .textFieldStyle(RoundedBorderTextFieldStyle())
            }

            VStack(alignment: .leading, spacing: 4) {
                Text("Содержимое:")
                    .font(.caption)
                    .foregroundColor(.secondary)
                TextEditor(text: $content)
                    .font(.system(.body, design: .monospaced))
                    .frame(height: 180)
                    .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color.secondary.opacity(0.3)))
            }

            HStack {
                Spacer()

                Button("Отмена") {
                    dismiss()
                }

                Button("Сохранить") {
                    saveSnippet()
                    dismiss()
                }
                .buttonStyle(.borderedProminent)
                .disabled(content.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        }
        .padding(20)
        .frame(width: 460)
        .onAppear {
            if let item = editingItem {
                self.title = item.title
                self.content = item.content
            }
        }
    }

    private func saveSnippet() {
        let cleanContent = content.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !cleanContent.isEmpty else { return }

        if let item = editingItem {
            appState.storage.updateItem(id: item.id, title: title, content: cleanContent)
        } else {
            let item = appState.storage.addOrUpdate(content: cleanContent, title: title)
            appState.storage.togglePin(id: item.id) // new created snippets start pinned
        }
        appState.refresh()
    }
}
