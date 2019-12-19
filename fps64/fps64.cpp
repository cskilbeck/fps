//////////////////////////////////////////////////////////////////////

#include "framework.h"
#include "..\fps_dll\fps_dll.h"
#include "fps64.h"

//////////////////////////////////////////////////////////////////////

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    if(!already_running()) {
        HANDLE e = CreateEvent(null, true, false, global_event_name);
        install_kbd_hook();
        WaitForSingleObject(e, INFINITE);
        uninstall_kbd_hook();
    }
}
