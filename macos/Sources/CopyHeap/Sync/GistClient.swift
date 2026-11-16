import Foundation

public final class GistClient {
    private static let gistFileName = "copyheap_pinned.json"
    private static let baseURL = URL(string: "https://api.github.com")!

    public struct GistResponse: Codable {
        public let id: String
        public let description: String?
        public let files: [String: GistFile]
    }

    public struct GistFile: Codable {
        public let filename: String?
        public let content: String?
        public let raw_url: String?
    }

    public static func findOrCreateGist(token: String, existingGistId: String?) async throws -> (gistId: String, content: String?) {
        if let existingGistId = existingGistId, !existingGistId.isEmpty {
            if let result = try? await getGist(token: token, gistId: existingGistId) {
                return (existingGistId, result)
            }
        }

        // Search in user's gists
        let gists = try await listGists(token: token)
        for gist in gists {
            if gist.files[gistFileName] != nil || gist.description == "CopyHeap Pinned Snippets Backup" {
                let content = gist.files[gistFileName]?.content
                return (gist.id, content)
            }
        }

        // Create new secret gist
        let newGistId = try await createGist(token: token, initialContent: "[]")
        return (newGistId, "[]")
    }

    public static func listGists(token: String) async throws -> [GistResponse] {
        var request = URLRequest(url: baseURL.appendingPathComponent("gists"))
        request.setValue("token \(token)", forHTTPHeaderField: "Authorization")
        request.setValue("application/vnd.github.v3+json", forHTTPHeaderField: "Accept")
        request.setValue("CopyHeap-macOS/1.0", forHTTPHeaderField: "User-Agent")

        let (data, response) = try await URLSession.shared.data(for: request)
        guard let httpResponse = response as? HTTPURLResponse, (200...299).contains(httpResponse.statusCode) else {
            throw NSError(domain: "GistClient", code: 1, userInfo: [NSLocalizedDescriptionKey: "Failed to list gists"])
        }
        return try JSONDecoder().decode([GistResponse].self, from: data)
    }

    public static func getGist(token: String, gistId: String) async throws -> String? {
        let url = baseURL.appendingPathComponent("gists").appendingPathComponent(gistId)
        var request = URLRequest(url: url)
        request.setValue("token \(token)", forHTTPHeaderField: "Authorization")
        request.setValue("application/vnd.github.v3+json", forHTTPHeaderField: "Accept")
        request.setValue("CopyHeap-macOS/1.0", forHTTPHeaderField: "User-Agent")

        let (data, response) = try await URLSession.shared.data(for: request)
        guard let httpResponse = response as? HTTPURLResponse, (200...299).contains(httpResponse.statusCode) else {
            throw NSError(domain: "GistClient", code: 2, userInfo: [NSLocalizedDescriptionKey: "Failed to get gist \(gistId)"])
        }
        let gist = try JSONDecoder().decode(GistResponse.self, from: data)
        return gist.files[gistFileName]?.content
    }

    public static func createGist(token: String, initialContent: String) async throws -> String {
        let url = baseURL.appendingPathComponent("gists")
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("token \(token)", forHTTPHeaderField: "Authorization")
        request.setValue("application/vnd.github.v3+json", forHTTPHeaderField: "Accept")
        request.setValue("CopyHeap-macOS/1.0", forHTTPHeaderField: "User-Agent")
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")

        let body: [String: Any] = [
            "description": "CopyHeap Pinned Snippets Backup",
            "public": false,
            "files": [
                gistFileName: [
                    "content": initialContent
                ]
            ]
        ]
        request.httpBody = try JSONSerialization.data(withJSONObject: body)

        let (data, response) = try await URLSession.shared.data(for: request)
        guard let httpResponse = response as? HTTPURLResponse, (200...299).contains(httpResponse.statusCode) else {
            throw NSError(domain: "GistClient", code: 3, userInfo: [NSLocalizedDescriptionKey: "Failed to create gist"])
        }
        let created = try JSONDecoder().decode(GistResponse.self, from: data)
        return created.id
    }

    public static func updateGist(token: String, gistId: String, content: String) async throws {
        let url = baseURL.appendingPathComponent("gists").appendingPathComponent(gistId)
        var request = URLRequest(url: url)
        request.httpMethod = "PATCH"
        request.setValue("token \(token)", forHTTPHeaderField: "Authorization")
        request.setValue("application/vnd.github.v3+json", forHTTPHeaderField: "Accept")
        request.setValue("CopyHeap-macOS/1.0", forHTTPHeaderField: "User-Agent")
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")

        let body: [String: Any] = [
            "files": [
                gistFileName: [
                    "content": content
                ]
            ]
        ]
        request.httpBody = try JSONSerialization.data(withJSONObject: body)

        let (_, response) = try await URLSession.shared.data(for: request)
        guard let httpResponse = response as? HTTPURLResponse, (200...299).contains(httpResponse.statusCode) else {
            throw NSError(domain: "GistClient", code: 4, userInfo: [NSLocalizedDescriptionKey: "Failed to update gist \(gistId)"])
        }
    }
}
