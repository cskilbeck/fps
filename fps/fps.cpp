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

HANDLE global_mutex_handle = null;
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

void set_exit_event()
{
    HANDLE e = CreateEvent(null, true, false, global_event_name);
    SetEvent(e);
}

//////////////////////////////////////////////////////////////////////

void run_64bit()
{
    // run 64 bit exe, it will stop itself running more than once
    // it will connect to the pipe and send messages that way
    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    CreateProcess("fps64.exe", null, null, null, false, 0, null, null, &si, &pi);
}

//////////////////////////////////////////////////////////////////////

// you can murder this thread by closing the pipe_handle
DWORD WINAPI handle_pipe_messages(void *)
{
    OutputDebugString("pipe handler starts\n");
    pipe_handle = CreateNamedPipe(pipe_name, PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS | PIPE_READMODE_MESSAGE,
                                  PIPE_UNLIMITED_INSTANCES, pipe_buffer_size_bytes, pipe_buffer_size_bytes, 1000, null);
    if(pipe_handle == INVALID_HANDLE_VALUE) {
        OutputDebugString("Error creating pipe\n");
        return 0;
    }
    if(!ConnectNamedPipe(pipe_handle, null) && GetLastError() != ERROR_PIPE_CONNECTED) {
        OutputDebugString("Error waitinf for pipe connection\n");
        CloseHandle(pipe_handle);
        return 0;
    }
    char pipe_buffer[pipe_buffer_size_bytes];
    OutputDebugString("handle_pipe_messages\n");
    DWORD got;
    while(true) {
        if(!ReadFile(pipe_handle, pipe_buffer, pipe_buffer_size_bytes, &got, null)) {
            OutputDebugString("pipe closed!?\n");
            break;
        }
        OutputDebugString("MSG: ");
        OutputDebugString(pipe_buffer);
        OutputDebugString("\n");

        HWND hEdit = GetDlgItem(main_dlg, 1002);
        SendMessage(hEdit, EM_SETSEL, LONG_MAX, LONG_MAX);
        SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)got);
    }
    OutputDebugString("pipe handler thread exiting\n");
    return 0;
}

//////////////////////////////////////////////////////////////////////

void create_pipe()
{
    DWORD thread_id;
    pipe_handler_thread = CreateThread(null, 0, handle_pipe_messages, null, 0, &thread_id);
}

//////////////////////////////////////////////////////////////////////
// DLG PROC

INT_PTR CALLBACK dlg_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {

    case WM_INITDIALOG:
        main_dlg = hDlg;
        create_pipe();
        // run_64bit();
        install_kbd_hook();
        SetTimer(hDlg, 1, 10, null);
        break;

    case WM_TIMER:
        KillTimer(hDlg, 1);
        SendMessage(GetDlgItem(hDlg, IDC_EDIT1), EM_SETSEL, -1, -1);    // ffs remove selection from edit control
        break;

    case WM_SIZE:
    case WM_SIZING: {
        RECT r;
        HWND edit_ctrl = GetDlgItem(hDlg, IDC_EDIT1);
        GetClientRect(hDlg, &r);
        MoveWindow(edit_ctrl, 0, 0, r.right, r.bottom, TRUE);
    } break;

    case WM_COMMAND:
        switch(LOWORD(wParam)) {
        case ID_LICENSE_KIERO:
            // log(licenses::text());
            break;
        }
        break;

    case WM_CLOSE:
        set_exit_event();
        // DisconnectNamedPipe(pipe_handle);
        // CloseHandle(pipe_handle);
        // if(WaitForSingleObject(pipe_handler_thread, 500) == WAIT_ABANDONED) {
        //    TerminateThread(pipe_handler_thread, 0);
        //}
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
        CloseHandle(global_mutex_handle);
    }
    return 0;
}
