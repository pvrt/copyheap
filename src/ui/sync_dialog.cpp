#include "sync_dialog.h"
#include "core/string_utils.h"

#include <commctrl.h>
#include <ctime>

namespace copyheap {

constexpr int IDC_SYNC_TOKEN_EDIT    = 501;
constexpr int IDC_SYNC_AUTO_CHECK    = 502;
constexpr int IDC_SYNC_NOW_BTN       = 503;
constexpr int IDC_SYNC_SAVE_BTN      = 504;
constexpr int IDC_SYNC_CLOSE_BTN     = 505;

static const wchar_t* SYNC_DIALOG_CLASS = L"CopyHeap_SyncDialog";

SyncDialog::SyncDialog(HINSTANCE hInstance, HWND hParent, SyncManager& syncMgr, ConfigManager& configMgr)
    : m_hInstance(hInstance), m_hParent(hParent), m_syncMgr(syncMgr), m_configMgr(configMgr)
{
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0);
    m_hFontUI = CreateFontIndirectW(&ncm.lfMessageFont);

    LOGFONTW lfHint = ncm.lfMessageFont;
    lfHint.lfHeight = (lfHint.lfHeight * 85) / 100;
    m_hFontHint = CreateFontIndirectW(&lfHint);
}

SyncDialog::~SyncDialog() {
    if (m_hFontUI) DeleteObject(m_hFontUI);
    if (m_hFontHint) DeleteObject(m_hFontHint);
    if (m_hwnd) DestroyWindow(m_hwnd);
}

void SyncDialog::show() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = SYNC_DIALOG_CLASS;
    RegisterClassExW(&wc);

    int w = 420;
    int h = 260;

    RECT rcParent{};
    int x = 200, y = 200;
    if (m_hParent && GetWindowRect(m_hParent, &rcParent)) {
        x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
        y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        SYNC_DIALOG_CLASS,
        L"Синхронизация с GitHub Gist",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h,
        m_hParent,
        nullptr,
        m_hInstance,
        this
    );

    EnableWindow(m_hParent, FALSE);

    MSG msg;
    while (IsWindow(m_hwnd) && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (m_hParent) {
        EnableWindow(m_hParent, TRUE);
        SetForegroundWindow(m_hParent);
    }
}

LRESULT CALLBACK SyncDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SyncDialog* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SyncDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<SyncDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->handle_message(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void SyncDialog::init_controls() {
    const auto& cfg = m_configMgr.get_config();

    // 1. Label
    m_hTokenLabel = CreateWindowExW(
        0, L"STATIC", L"GitHub Personal Access Token (с правами 'gist'):",
        WS_CHILD | WS_VISIBLE,
        20, 16, 370, 18,
        m_hwnd, nullptr, m_hInstance, nullptr
    );
    SendMessageW(m_hTokenLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // 2. Token Edit
    m_hTokenEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", utf8_to_wide(cfg.github_token).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD,
        20, 38, 365, 23,
        m_hwnd, (HMENU)(INT_PTR)IDC_SYNC_TOKEN_EDIT, m_hInstance, nullptr
    );
    SendMessageW(m_hTokenEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SendMessageW(m_hTokenEdit, EM_SETCUEBANNER, TRUE, (LPARAM)L"ghp_... или github_pat_...");

    // 3. Hint
    m_hHintLabel = CreateWindowExW(
        0, L"STATIC", L"Секретный Gist 'copyheap_pinned.json' создаётся автоматически.",
        WS_CHILD | WS_VISIBLE,
        20, 66, 370, 16,
        m_hwnd, nullptr, m_hInstance, nullptr
    );
    SendMessageW(m_hHintLabel, WM_SETFONT, (WPARAM)m_hFontHint, TRUE);

    // 4. Auto sync checkbox
    m_hAutoSyncCheck = CreateWindowExW(
        0, L"BUTTON", L"Включить синхронизацию закреплённых элементов",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        20, 92, 365, 20,
        m_hwnd, (HMENU)(INT_PTR)IDC_SYNC_AUTO_CHECK, m_hInstance, nullptr
    );
    SendMessageW(m_hAutoSyncCheck, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SendMessageW(m_hAutoSyncCheck, BM_SETCHECK, cfg.sync_enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    // 5. Status label
    m_hStatusLabel = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE,
        20, 122, 365, 20,
        m_hwnd, nullptr, m_hInstance, nullptr
    );
    SendMessageW(m_hStatusLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    update_status_text();

    // 6. Buttons
    m_hSyncNowBtn = CreateWindowExW(
        0, L"BUTTON", L"Синхронизировать сейчас",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        20, 165, 175, 28,
        m_hwnd, (HMENU)(INT_PTR)IDC_SYNC_NOW_BTN, m_hInstance, nullptr
    );
    SendMessageW(m_hSyncNowBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    m_hSaveBtn = CreateWindowExW(
        0, L"BUTTON", L"Сохранить",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        205, 165, 95, 28,
        m_hwnd, (HMENU)(INT_PTR)IDC_SYNC_SAVE_BTN, m_hInstance, nullptr
    );
    SendMessageW(m_hSaveBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    m_hCloseBtn = CreateWindowExW(
        0, L"BUTTON", L"Закрыть",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        308, 165, 77, 28,
        m_hwnd, (HMENU)(INT_PTR)IDC_SYNC_CLOSE_BTN, m_hInstance, nullptr
    );
    SendMessageW(m_hCloseBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
}

void SyncDialog::update_status_text() {
    const auto& cfg = m_configMgr.get_config();
    std::wstring status;

    if (!cfg.sync_enabled || cfg.github_token.empty()) {
        status = L"Статус: Синхронизация не настроена";
    } else if (cfg.last_sync_time > 0) {
        time_t t = static_cast<time_t>(cfg.last_sync_time / 1000);
        tm ltm{};
        localtime_s(&ltm, &t);
        wchar_t buf[64];
        wcsftime(buf, 64, L"%d.%m.%Y %H:%M:%S", &ltm);
        status = L"Последняя синхронизация: " + std::wstring(buf);
    } else {
        status = L"Статус: Готов к синхронизации";
    }

    SetWindowTextW(m_hStatusLabel, status.c_str());
}

void SyncDialog::on_save_clicked() {
    wchar_t tokenBuf[256] = {0};
    GetWindowTextW(m_hTokenEdit, tokenBuf, 256);
    std::string token = wide_to_utf8(tokenBuf);

    bool autoSync = (SendMessageW(m_hAutoSyncCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);

    if (autoSync && !token.empty()) {
        SetWindowTextW(m_hStatusLabel, L"Проверка токена и настройка Gist...");
        UpdateWindow(m_hStatusLabel);

        std::string err;
        bool ok = m_syncMgr.setup_sync(token, err);
        if (!ok) {
            std::wstring wErr = utf8_to_wide(err);
            MessageBoxW(m_hwnd, wErr.c_str(), L"Ошибка подключения", MB_OK | MB_ICONERROR);
            SetWindowTextW(m_hStatusLabel, L"Ошибка авторизации");
            return;
        }

        SetWindowTextW(m_hStatusLabel, L"Токен проверен. Настройки сохранены.");
    } else {
        m_configMgr.set_github_token(token);
        m_configMgr.set_sync_enabled(autoSync);
        m_configMgr.save();
        update_status_text();
    }
}

void SyncDialog::on_sync_now_clicked() {
    on_save_clicked();

    SetWindowTextW(m_hStatusLabel, L"Синхронизация...");
    EnableWindow(m_hSyncNowBtn, FALSE);

    HWND hwnd = m_hwnd;
    HWND statusLbl = m_hStatusLabel;
    HWND syncBtn = m_hSyncNowBtn;

    m_syncMgr.trigger_sync([hwnd, statusLbl, syncBtn, this](SyncStatus status, const std::string& msg) {
        PostMessageW(hwnd, WM_USER + 201, (WPARAM)status, 0);
    });
}

LRESULT SyncDialog::handle_message(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            init_controls();
            return 0;

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_SYNC_SAVE_BTN) {
                on_save_clicked();
                return 0;
            }
            if (id == IDC_SYNC_NOW_BTN) {
                on_sync_now_clicked();
                return 0;
            }
            if (id == IDC_SYNC_CLOSE_BTN || id == IDCANCEL) {
                DestroyWindow(m_hwnd);
                return 0;
            }
            break;
        }

        case WM_USER + 201: {
            EnableWindow(m_hSyncNowBtn, TRUE);
            update_status_text();
            std::string msgStr = m_syncMgr.get_last_message();
            SetWindowTextW(m_hStatusLabel, utf8_to_wide(msgStr).c_str());
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(m_hwnd);
            return 0;

        case WM_DESTROY:
            m_hwnd = nullptr;
            return 0;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

} // namespace copyheap
