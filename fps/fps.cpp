//////////////////////////////////////////////////////////////////////
// Get fps64 running
// Talk to fps64
// Hook both D3D9 and D3D11 always
// Check it works when multiple hooks installed (shared dataseg admin)
// Minimize to systray
// Drawing (9 & 11) with minimal impact on GPU

#include <windows.h>
#include <xinput.h>
#include <stdio.h>
#include "resource.h"
#include "licenses.h"
#include "..\fps_dll\minhook\include\MinHook.h"
#include "..\fps_dll\kiero.h"
#include "..\fps_dll\fps_dll.h"
#include "..\fps_dll\defer.h"

//////////////////////////////////////////////////////////////////////

#if defined(_WIN64)
char const *d3d9_text = "D3D9 - 64 bit";
char const *d3d11_text = "D3D11 - 64 bit";
#else
char const *d3d9_text = "D3D9 - 32 bit";
char const *d3d11_text = "D3D11 - 32 bit";
#endif

char const *keypath = "Software\\chs\\fps";

HWND main_dlg = null;
HANDLE pipe_handle;
HANDLE pipe_handler_thread = null;

//////////////////////////////////////////////////////////////////////
// REGISTRY STUFF

bool save_dword(char const *name, DWORD value)
{
    bool rc = true;
    HKEY key;
    if(FAILED(RegCreateKeyEx(HKEY_CURRENT_USER, keypath, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL))) {
        log("Can't save %s to registry, RegOpenKeyEx failed: %08x", name, GetLastError());
        return false;
    }
    if(FAILED(RegSetValueEx(key, name, 0, REG_DWORD, (const BYTE *)&value, sizeof(DWORD)))) {
        log("Can't save %s to registry, RegSetValueEx failed: %08x", name, GetLastError());
        rc = false;
    }
    RegCloseKey(key);
    return rc;
}

//////////////////////////////////////////////////////////////////////

DWORD load_dword(char const *name, DWORD default_value)
{
    DWORD type;
    DWORD len = sizeof(DWORD);
    DWORD value = default_value;
    LSTATUS s = RegGetValue(HKEY_CURRENT_USER, keypath, name, RRF_RT_DWORD, &type, (LPVOID)&value, &len);
    if(s != ERROR_SUCCESS) {
        log("%s not found in registry, defaulting to %d", name, default_value);
    }
    return value;
}

//////////////////////////////////////////////////////////////////////

HANDLE exit_event;

//////////////////////////////////////////////////////////////////////

void run_64bit()
{
    // run 64 bit exe, it will stop itself running more than once
    // it will connect to the pipe and send messages that way
    char module_path[MAX_PATH];
    char module_filename[MAX_PATH];
    char *filepart;
    GetModuleFileName(NULL, module_filename, MAX_PATH);
    GetFullPathName(module_filename, MAX_PATH, module_path, &filepart);
    if(filepart != null) {
        *filepart = 0;
    }
    SetCurrentDirectory(module_path);
    debug_log("Changing directory to %s", module_path);
    debug_log("Spawning x64 handler");
    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    if(CreateProcess("fps64_x64.exe", null, null, null, false, 0, null, null, &si, &pi) == 0) {
        debug_log("CreateProcess failed: %08x", GetLastError());
    }
}

//////////////////////////////////////////////////////////////////////

HANDLE pipe_thread_start_event;

void create_pipe()
{
    pipe_thread_start_event = CreateEvent(null, true, false, null);
    extern DWORD WINAPI pipe_server(void *exit_event_arg);
    pipe_handler_thread = CreateThread(null, 0, pipe_server, (void *)exit_event, 0, null);
    if(pipe_handler_thread == null) {
        debug_log("ERROR CreateThread(pipe_server): %08x", GetLastError());
    } else if(pipe_thread_start_event != null) {
        WaitForSingleObject(pipe_thread_start_event, 1000);
        CloseHandle(pipe_thread_start_event);
    }
}

//////////////////////////////////////////////////////////////////////
// DLG PROC

INT_PTR CALLBACK dlg_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {

    case WM_INITDIALOG:
        debug_log("WM_INITDIALOG");
        main_dlg = hDlg;
        exit_event = CreateEvent(null, true, false, global_event_name);
        create_pipe();
        run_64bit();
        install_kbd_hook();
        SetTimer(hDlg, 1, 50, null);
        break;

    case WM_TIMER: {
        KillTimer(hDlg, 1);
        HWND edit_ctrl = GetDlgItem(hDlg, IDC_EDIT1);
        SendMessage(edit_ctrl, EM_SETSEL, LONG_MAX, LONG_MAX);
    } break;

    case WM_SIZE:
    case WM_SIZING: {
        RECT r;
        HWND edit_ctrl = GetDlgItem(hDlg, IDC_EDIT1);
        GetClientRect(hDlg, &r);
        MoveWindow(edit_ctrl, 0, 0, r.right, r.bottom, TRUE);
    } break;

    case WM_USER: {
        HWND hEdit = GetDlgItem(main_dlg, IDC_EDIT1);
        SendMessage(hEdit, EM_SETSEL, LONG_MAX, LONG_MAX);
        SendMessage(hEdit, EM_REPLACESEL, 0, lParam);
        OutputDebugString((char const *)lParam);
        delete[](char *) lParam;    // lame!
    } break;

    case WM_COMMAND:
        switch(LOWORD(wParam)) {
        case ID_LICENSE_KIERO:
            log_raw(licenses::text, licenses::len);
            break;
        }
        break;

    case WM_CLOSE:
        SetEvent(exit_event);
        if(WaitForSingleObject(pipe_handler_thread, 1000) == WAIT_ABANDONED) {
            TerminateThread(pipe_handler_thread, 0);
        }
        uninstall_kbd_hook();
        EndDialog(hDlg, 0);
        break;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    if(already_running()) {
        MessageBox(null, "It's already running!", "FPS", MB_ICONINFORMATION);
    } else {
        DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_WININFO), NULL, dlg_proc, (LPARAM)NULL);
    }
    return 0;
}
