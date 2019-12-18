//////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <xinput.h>
#include <stdio.h>
#include "resource.h"
#include "licenses.h"
#include "..\fps_dll\minhook\include\MinHook.h"
#include "..\fps_dll\kiero.h"
#include "..\fps_dll\fps_dll.h"

//////////////////////////////////////////////////////////////////////

#if defined(_WIN64)
char const *d3d9_text = "D3D9 - 64 bit";
char const *d3d11_text = "D3D11 - 64 bit";
#else
char const *d3d9_text = "D3D9 - 32 bit";
char const *d3d11_text = "D3D11 - 32 bit";
#endif

frame_timings *timings = null;

char const *keypath = "Software\\chs\\flash_hooker";
char const *d3d_keyname = "D3DType";
char const *button_mask_keyname = "Buttons";

uint16_t buttons[] = { XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y };
char const *button_names[] = { "A", "B", "X", "Y" };

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
// D3D MODE SET

void set_d3d(HWND dlg, UINT id_on, UINT id_off, char const *window_txt, kiero::RenderType new_render_type)
{
    HMENU menu = GetMenu(dlg);
    set_render_type(new_render_type);
    CheckMenuItem(menu, id_off, MF_UNCHECKED);
    CheckMenuItem(menu, id_on, MF_CHECKED);
    SetWindowText(dlg, window_txt);
}

void set_d3d9_mode(HWND hDlg)
{
    log("Setting D3D9 mode");
    set_d3d(hDlg, ID_D3D_9, ID_D3D_11, d3d9_text, kiero::RenderType::D3D9);
}

void set_d3d11_mode(HWND hDlg)
{
    log("Setting D3D11 mode");
    set_d3d(hDlg, ID_D3D_11, ID_D3D_9, d3d11_text, kiero::RenderType::D3D11);
}

//////////////////////////////////////////////////////////////////////

void load_d3d_state(HWND dlg)
{
    switch(load_dword(d3d_keyname, kiero::RenderType::D3D11)) {
    case kiero::RenderType::D3D11:
        set_d3d11_mode(dlg);
        break;
    case kiero::RenderType::D3D9:
        set_d3d9_mode(dlg);
        break;
    }
}

//////////////////////////////////////////////////////////////////////

void save_d3d_state()
{
    save_dword(d3d_keyname, get_render_type());
}

//////////////////////////////////////////////////////////////////////
// BUTTONS

void update_button_menu(HWND hDlg)
{
    int button_menu_entries[] = { ID_BUTTONS_A, ID_BUTTONS_B, ID_BUTTONS_X, ID_BUTTONS_Y };
    int buttons[] = { XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y };

    uint32_t mask = get_button_mask();
    HMENU menu = GetMenu(hDlg);

    for(int i = 0; i < 4; ++i) {
        bool checked = (buttons[i] & mask) != 0;
        CheckMenuItem(menu, button_menu_entries[i], MF_BYCOMMAND | ((checked ? MF_CHECKED : MF_UNCHECKED)));
    }
}

//////////////////////////////////////////////////////////////////////
// a set bit in mask toggles that bit in button_mask

void toggle_button(HWND hDlg, uint32_t toggle_bits)
{
    uint32_t current_buttons = get_button_mask() ^ toggle_bits;
    if(current_buttons == 0) {
        log("WARNING! BUTTON MASK IS EMPTY, IT WILL NEVER FLASH!");
    }
    setup_buttons(current_buttons);
    update_button_menu(hDlg);
}

//////////////////////////////////////////////////////////////////////

void load_buttons(HWND dlg)
{
    setup_buttons(load_dword(button_mask_keyname, XINPUT_GAMEPAD_A));
    update_button_menu(dlg);
}

//////////////////////////////////////////////////////////////////////

void save_buttons()
{
    save_dword(button_mask_keyname, get_button_mask());
}

//////////////////////////////////////////////////////////////////////
// SESSION SAVE

void do_session_save(HWND hwnd)
{
    if(timings == null || timings->num_timings == 0) {
        log("Can't save an empty session!?");
        MessageBox(null, "No session data has been recorded yet", "No session data", MB_ICONEXCLAMATION);
        return;
    }

    OPENFILENAME ofn;
    char filename[MAX_PATH];

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = filename;
    ofn.lpstrFile[0] = '\0';    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not use the contents of szFile to initialize itself.
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = "All (*.*)\0*.*\0CSV (*.csv)\0*.csv\0";
    ofn.nFilterIndex = 2;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrTitle = "Save session data to CSV file";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN;

    if(GetSaveFileName(&ofn) == FALSE) {
        log("Save session filename dialog cancelled");
    } else {
        log("Saving %d frame times to to %s", timings->num_timings, filename);
        FILE *f;
        errno_t e = fopen_s(&f, filename, "w");
        if(f == null) {
            log("Error opening file: %d", e);
            return;
        }
        for(int i = 0; i < timings->num_timings; ++i) {
            fprintf(f, "%f\n", timings->timings[i] * 0.0000001);
        }
        fclose(f);
        log("Done");
    }
}

//////////////////////////////////////////////////////////////////////
// DLG PROC

INT_PTR CALLBACK dlg_proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg) {

    case WM_INITDIALOG:
        install_kbd_hook(hDlg);
        load_d3d_state(hDlg);
        load_buttons(hDlg);
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

    case WM_USER: {
        // LPARAM: ptr to frame timing buffer
        // WPARAM: ptr to frame timing counter
        if(timings == null) {
            log("Got frame timings at %p", lParam);
            timings = reinterpret_cast<frame_timings *>(lParam);
        }
    } break;

    case WM_COMMAND:
        switch(LOWORD(wParam)) {
        case ID_BUTTONS_A:
            toggle_button(hDlg, XINPUT_GAMEPAD_A);
            break;
        case ID_BUTTONS_B:
            toggle_button(hDlg, XINPUT_GAMEPAD_B);
            break;
        case ID_BUTTONS_X:
            toggle_button(hDlg, XINPUT_GAMEPAD_X);
            break;
        case ID_BUTTONS_Y:
            toggle_button(hDlg, XINPUT_GAMEPAD_Y);
            break;
        case ID_D3D_9:
            set_d3d9_mode(hDlg);
            break;
        case ID_D3D_11:
            set_d3d11_mode(hDlg);
            break;
        case ID_LICENSE_KIERO:
            log_raw(licenses::text());
            break;
        case ID_SESSION_SAVE:
            do_session_save(hDlg);
            break;
        }
        break;

    case WM_CLOSE:
        save_d3d_state();
        save_buttons();
        uninstall_kbd_hook();
        EndDialog(hDlg, 0);
        break;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_WININFO), NULL, dlg_proc, (LPARAM)NULL);
    return 0;
}
