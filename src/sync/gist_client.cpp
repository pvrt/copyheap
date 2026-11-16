#include "gist_client.h"
#include "crypto_utils.h"
#include "core/storage_manager.h"
#include "core/string_utils.h"

#include <windows.h>
#include <winhttp.h>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace copyheap {

static const wchar_t* GITHUB_HOST = L"api.github.com";
static const wchar_t* GIST_FILE_NAME = L"copyheap_pinned.json";

GistClient::HttpResponse GistClient::send_request(
    const std::wstring& verb,
    const std::wstring& path,
    const std::string& token,
    const std::string& body)
{
    HttpResponse response;

    HINTERNET hSession = WinHttpOpen(
        L"CopyHeap-Sync/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) return response;

    // GitHub requires TLS 1.2+
    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

    HINTERNET hConnect = WinHttpConnect(hSession, GITHUB_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return response;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        verb.c_str(),
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }

    std::wstring headers = L"Accept: application/vnd.github+json\r\n";
    headers += L"X-GitHub-Api-Version: 2022-11-28\r\n";
    if (!token.empty()) {
        headers += L"Authorization: Bearer " + utf8_to_wide(token) + L"\r\n";
    }
    if (!body.empty()) {
        headers += L"Content-Type: application/json; charset=utf-8\r\n";
    }

    BOOL bResults = WinHttpSendRequest(
        hRequest,
        headers.c_str(),
        static_cast<DWORD>(headers.length()),
        body.empty() ? nullptr : (LPVOID)body.data(),
        static_cast<DWORD>(body.length()),
        static_cast<DWORD>(body.length()),
        0
    );

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, nullptr);
    }

    if (bResults) {
        DWORD statusCode = 0;
        DWORD size = sizeof(statusCode);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &size,
            WINHTTP_NO_HEADER_INDEX
        );
        response.status_code = static_cast<int>(statusCode);

        DWORD dwSize = 0;
        do {
            dwSize = 0;
            if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                std::vector<char> buffer(dwSize + 1);
                DWORD dwDownloaded = 0;
                if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                    response.body.append(buffer.data(), dwDownloaded);
                }
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

bool GistClient::validate_token(const std::string& token) {
    if (token.empty()) return false;
    HttpResponse res = send_request(L"GET", L"/user", token);
    return res.status_code == 200;
}

std::optional<std::string> GistClient::find_existing_gist(const std::string& token) {
    if (token.empty()) return std::nullopt;

    HttpResponse res = send_request(L"GET", L"/gists?per_page=50", token);
    if (res.status_code != 200 || res.body.empty()) {
        return std::nullopt;
    }

    // Simple JSON search for "copyheap_pinned.json" or "CopyHeap Pinned Snippets"
    const std::string& body = res.body;
    size_t targetPos = body.find("copyheap_pinned.json");
    if (targetPos == std::string::npos) {
        targetPos = body.find("CopyHeap Pinned Snippets");
    }

    if (targetPos != std::string::npos) {
        // Find the "id": "..." of this gist object by scanning backwards
        size_t idPos = body.rfind("\"id\":", targetPos);
        if (idPos != std::string::npos) {
            size_t openQuote = body.find('"', idPos + 5);
            if (openQuote != std::string::npos) {
                size_t closeQuote = body.find('"', openQuote + 1);
                if (closeQuote != std::string::npos) {
                    return body.substr(openQuote + 1, closeQuote - openQuote - 1);
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<std::string> GistClient::create_gist(const std::string& token, const std::string& initial_content) {
    if (token.empty()) return std::nullopt;

    // Encrypt content with AES-256 using key derived from token
    std::string payload = CryptoUtils::encrypt_aes256(initial_content, token);

    std::ostringstream ss;
    ss << "{\n"
       << "  \"description\": \"CopyHeap Pinned Snippets\",\n"
       << "  \"public\": false,\n"
       << "  \"files\": {\n"
       << "    \"copyheap_pinned.json\": {\n"
       << "      \"content\": \"" << json_escape(payload) << "\"\n"
       << "    }\n"
       << "  }\n"
       << "}\n";

    HttpResponse res = send_request(L"POST", L"/gists", token, ss.str());
    if (res.status_code != 201 || res.body.empty()) {
        return std::nullopt;
    }

    // Extract "id"
    size_t idPos = res.body.find("\"id\":");
    if (idPos != std::string::npos) {
        size_t openQuote = res.body.find('"', idPos + 5);
        if (openQuote != std::string::npos) {
            size_t closeQuote = res.body.find('"', openQuote + 1);
            if (closeQuote != std::string::npos) {
                return res.body.substr(openQuote + 1, closeQuote - openQuote - 1);
            }
        }
    }

    return std::nullopt;
}

std::optional<std::vector<ClipboardItem>> GistClient::pull_pinned(const std::string& token, const std::string& gist_id) {
    if (token.empty() || gist_id.empty()) return std::nullopt;

    std::wstring path = L"/gists/" + utf8_to_wide(gist_id);
    HttpResponse res = send_request(L"GET", path, token);
    if (res.status_code != 200 || res.body.empty()) {
        return std::nullopt;
    }

    // Look for "copyheap_pinned.json" -> "content": "..."
    size_t filePos = res.body.find("copyheap_pinned.json");
    if (filePos == std::string::npos) {
        return std::nullopt;
    }

    size_t contentPos = res.body.find("\"content\":", filePos);
    if (contentPos == std::string::npos) {
        return std::nullopt;
    }

    size_t openQuote = res.body.find('"', contentPos + 10);
    if (openQuote == std::string::npos) return std::nullopt;

    // Extract raw string with escape decoding
    std::string unescaped;
    size_t pos = openQuote + 1;
    while (pos < res.body.size()) {
        char c = res.body[pos++];
        if (c == '"') break;
        if (c == '\\' && pos < res.body.size()) {
            char esc = res.body[pos++];
            switch (esc) {
                case '"': unescaped += '"'; break;
                case '\\': unescaped += '\\'; break;
                case '/': unescaped += '/'; break;
                case 'n': unescaped += '\n'; break;
                case 'r': unescaped += '\r'; break;
                case 't': unescaped += '\t'; break;
                default: unescaped += esc; break;
            }
        } else {
            unescaped += c;
        }
    }

    // If payload is encrypted, decrypt with key derived from token.
    // Otherwise fallback to unencrypted parsing for backwards compatibility.
    if (unescaped.find("\"encrypted\":") != std::string::npos &&
        unescaped.find("true") != std::string::npos) {
        auto decrypted = CryptoUtils::decrypt_aes256(unescaped, token);
        if (decrypted) {
            return StorageManager::deserialize_pinned_json(*decrypted);
        } else {
            return std::nullopt; // Bad key or corrupted data
        }
    }

    return StorageManager::deserialize_pinned_json(unescaped);
}

bool GistClient::push_pinned(const std::string& token, const std::string& gist_id, const std::string& json_content) {
    if (token.empty() || gist_id.empty()) return false;

    // Encrypt content with AES-256 using key derived from token
    std::string payload = CryptoUtils::encrypt_aes256(json_content, token);

    std::ostringstream ss;
    ss << "{\n"
       << "  \"description\": \"CopyHeap Pinned Snippets\",\n"
       << "  \"files\": {\n"
       << "    \"copyheap_pinned.json\": {\n"
       << "      \"content\": \"" << json_escape(payload) << "\"\n"
       << "    }\n"
       << "  }\n"
       << "}\n";

    std::wstring path = L"/gists/" + utf8_to_wide(gist_id);
    HttpResponse res = send_request(L"PATCH", path, token, ss.str());
    return res.status_code == 200;
}

} // namespace copyheap
