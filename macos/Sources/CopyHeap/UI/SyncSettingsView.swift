import SwiftUI

public struct SyncSettingsView: View {
    @ObservedObject var appState: AppState
    @Environment(\.dismiss) private var dismiss

    @State private var token: String = ""
    @State private var isSyncing: Bool = false
    @State private var syncStatusMessage: String = ""
    @State private var isError: Bool = false

    public var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack {
                Image(systemName: "arrow.triangle.2.circlepath")
                    .font(.system(size: 24, weight: .bold))
                    .foregroundColor(.accentColor)
                Text("Синхронизация через GitHub Gist")
                    .font(.headline)
            }

            Text("Синхронизируйте ваши закреплённые сниппеты между устройствами. Данные шифруются алгоритмом AES-256 (ключ формируется из вашего токена) и сохраняются в секретный Gist.")
                .font(.callout)
                .foregroundColor(.secondary)

            VStack(alignment: .leading, spacing: 6) {
                Text("GitHub Personal Access Token (classic):")
                    .font(.caption)
                    .foregroundColor(.secondary)

                SecureField("ghp_xxxxxxxxxxxxxxxxxxxx", text: $token)
                    .textFieldStyle(RoundedBorderTextFieldStyle())
            }

            if let lastSync = appState.config.lastSyncTime > 0 ? Date(timeIntervalSince1970: Double(appState.config.lastSyncTime) / 1000.0) : nil {
                Text("Последняя синхронизация: \(lastSync.formatted(date: .abbreviated, time: .standard))")
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }

            if !syncStatusMessage.isEmpty {
                Text(syncStatusMessage)
                    .font(.caption)
                    .foregroundColor(isError ? .red : .green)
            }

            Divider()

            HStack {
                Button("Инструкция по созданию токена") {
                    if let url = URL(string: "https://github.com/settings/tokens/new?scopes=gist&description=CopyHeap") {
                        NSWorkspace.shared.open(url)
                    }
                }
                .buttonStyle(.link)
                .font(.caption)

                Spacer()

                Button("Закрыть") {
                    dismiss()
                }

                Button(action: performSync) {
                    if isSyncing {
                        ProgressView().controlSize(.small)
                    } else {
                        Text("Синхронизировать сейчас")
                    }
                }
                .buttonStyle(.borderedProminent)
                .disabled(isSyncing || token.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
        }
        .padding(20)
        .frame(width: 480)
        .onAppear {
            self.token = appState.config.githubToken
        }
    }

    private func performSync() {
        let cleanToken = token.trimmingCharacters(in: .whitespacesAndNewlines)
        appState.configManager.updateConfig { $0.githubToken = cleanToken }
        isSyncing = true
        syncStatusMessage = "Выполняется синхронизация..."
        isError = false

        Task {
            do {
                try await appState.syncManager.syncNow()
                await MainActor.run {
                    self.isSyncing = false
                    self.syncStatusMessage = "Синхронизация успешно завершена!"
                    self.isError = false
                    self.appState.refresh()
                }
            } catch {
                await MainActor.run {
                    self.isSyncing = false
                    self.syncStatusMessage = error.localizedDescription
                    self.isError = true
                }
            }
        }
    }
}
