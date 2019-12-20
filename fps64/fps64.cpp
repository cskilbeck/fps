//////////////////////////////////////////////////////////////////////

#include "framework.h"
#include "..\fps_dll\fps_dll.h"
#include "fps64.h"

//////////////////////////////////////////////////////////////////////

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    if(already_running()) {
        log("x64 is already running!?");
    } else {
        debug_log("x64 starts");
        HANDLE e = CreateEvent(null, true, false, global_event_name);
        install_kbd_hook();
        WaitForSingleObject(e, INFINITE);
        debug_log("x64 quitting");
        uninstall_kbd_hook();
    }
}
