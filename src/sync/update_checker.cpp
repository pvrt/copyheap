#include "update_checker.h"
#include "core/string_utils.h"

#include <windows.h>
#include <winhttp.h>
#include <thread>

namespace copyheap {

static const wchar_t* GITHUB_HOST = L"api.github.com";
static const wchar_t* REPO_RELEASE_PATH = L"/repos/pvrt/copyheap/releases/latest";

std::optional<UpdateInfo> UpdateChecker::check_for_updates_sync() {
    HINTERNET hSession = WinHttpOpen(
        L"CopyHeap-UpdateChecker/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) return std::nullopt;

    DWORD secureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &secureProtocols, sizeof(secureProtocols));

    HINTERNET hConnect = WinHttpConnect(hSession, GITHUB_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        REPO_RELEASE_PATH,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    std::wstring headers = L"Accept: application/vnd.github+json\r\n";
    headers += L"X-GitHub-Api-Version: 2022-11-28\r\n";

    BOOL bResults = WinHttpSendRequest(
        hRequest,
        headers.c_str(),
        static_cast<DWORD>(headers.length()),
        nullptr, 0, 0, 0
    );

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, nullptr);
    }

    std::string body;
    DWORD statusCode = 0;
    if (bResults) {
        DWORD size = sizeof(statusCode);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &size,
            WINHTTP_NO_HEADER_INDEX
        );

        if (statusCode == 200) {
            DWORD dwSize = 0;
            do {
                dwSize = 0;
                if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                    std::vector<char> buffer(dwSize + 1);
                    DWORD dwDownloaded = 0;
                    if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                        body.append(buffer.data(), dwDownloaded);
                    }
                }
            } while (dwSize > 0);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (statusCode != 200 || body.empty()) {
        return std::nullopt;
    }

    auto extract_field = [&](const std::string& key) -> std::string {
        std::string pattern = "\"" + key + "\":";
        size_t pos = body.find(pattern);
        if (pos == std::string::npos) return "";
        pos = body.find('"', pos + pattern.size());
        if (pos == std::string::npos) return "";
        size_t end = body.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return body.substr(pos + 1, end - pos - 1);
    };

    std::string tagName = extract_field("tag_name");
    std::string htmlUrl = extract_field("html_url");

    if (tagName.empty()) {
        return std::nullopt;
    }

    UpdateInfo info;
    info.latest_version = tagName;
    info.release_url = htmlUrl.empty() ? "https://github.com/pvrt/copyheap/releases" : htmlUrl;

    // Compare versions (e.g. "v1.0.1" vs "v1.0.0")
    if (info.latest_version != CURRENT_VERSION && !info.latest_version.empty()) {
        info.update_available = true;
    }

    return info;
}

void UpdateChecker::check_for_updates_async(std::function<void(const UpdateInfo& info)> callback) {
    std::thread([callback = std::move(callback)]() {
        auto res = check_for_updates_sync();
        if (res && res->update_available) {
            if (callback) callback(*res);
        }
    }).detach();
}

} // namespace copyheap
