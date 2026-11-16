#pragma once

#include "core/models.h"

#include <string>
#include <vector>
#include <optional>

namespace copyheap {

class GistClient {
public:
    GistClient() = default;
    ~GistClient() = default;

    // Validate if the provided token can authenticate with GitHub API
    static bool validate_token(const std::string& token);

    // Find an existing gist with description "CopyHeap Pinned Snippets"
    // or containing file "copyheap_pinned.json"
    static std::optional<std::string> find_existing_gist(const std::string& token);

    // Create a new secret Gist containing copyheap_pinned.json
    static std::optional<std::string> create_gist(const std::string& token, const std::string& initial_content);

    // Fetch the pinned items from the specified Gist
    static std::optional<std::vector<ClipboardItem>> pull_pinned(const std::string& token, const std::string& gist_id);

    // Update the Gist with current pinned items
    static bool push_pinned(const std::string& token, const std::string& gist_id, const std::string& json_content);

private:
    struct HttpResponse {
        int status_code = 0;
        std::string body;
    };

    static HttpResponse send_request(
        const std::wstring& verb,
        const std::wstring& path,
        const std::string& token,
        const std::string& body = ""
    );
};

} // namespace copyheap
