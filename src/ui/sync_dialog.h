#pragma once

#include "sync/sync_manager.h"
#include "core/config_manager.h"

#include <windows.h>
#include <string>

namespace copyheap {

class SyncDialog {
public:
    SyncDialog(HINSTANCE hInstance, HWND hParent, SyncManager& syncMgr, ConfigManager& configMgr);
    ~SyncDialog();

    void show();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle_message(UINT msg, WPARAM wParam, LPARAM lParam);

    void init_controls();
    void update_status_text();
    void on_save_clicked();
    void on_sync_now_clicked();

    HINSTANCE m_hInstance;
    HWND m_hParent;
    HWND m_hwnd = nullptr;

    HWND m_hTokenLabel = nullptr;
    HWND m_hTokenEdit = nullptr;
    HWND m_hAutoSyncCheck = nullptr;
    HWND m_hStatusLabel = nullptr;
    HWND m_hSyncNowBtn = nullptr;
    HWND m_hSaveBtn = nullptr;
    HWND m_hCloseBtn = nullptr;
    HWND m_hHintLabel = nullptr;
    HWND m_hCreateTokenLink = nullptr;

    HFONT m_hFontUI = nullptr;
    HFONT m_hFontHint = nullptr;

    SyncManager& m_syncMgr;
    ConfigManager& m_configMgr;
};

} // namespace copyheap
