#include <windows.h>
#include <xinput.h>
#include "minhook/include/MinHook.h"
#include "kiero.h"
#include "fps_dll.h"
#include "controller.h"

void log(char const *text, ...);

namespace XInput {

typedef DWORD(WINAPI *XInputGetState_t)(DWORD, XINPUT_STATE *);

bool opened = false;
HMODULE XInputDll = null;
XInputGetState_t XInputGetStatePtr = null;

bool is_connected()
{
    return XInputGetStatePtr != null;
}

XInputGetState_t XInputGetState_fn()
{
    if(!opened) {
        log("Opening xinput1_3.dll");
        XInputDll = LoadLibrary("xinput1_3.DLL");
        if(XInputDll == null) {
            log("Error: xinput1_3.dll LoadLibrary() failed: %08x", GetLastError());
        } else {
            log("Loaded xinput1_3.dll: %p", XInputDll);
            XInputGetStatePtr = reinterpret_cast<XInputGetState_t>(GetProcAddress(XInputDll, "XInputGetState"));
            opened = true;
            if(XInputGetStatePtr == null) {
                log("XInput open failed, GetProcAddress() returned null");
            } else {
                log("xinput1_3.dll opened OK, XInputGetState: %p", XInputGetStatePtr);
            }
        }
    }
    return XInputGetStatePtr;
}

void cleanup()
{
    XInputGetStatePtr = null;
    if(XInputDll != null) {
        FreeLibrary(XInputDll);
        log("Closing XInput DLL");
    }
    opened = false;
}

DWORD WINAPI GetState(DWORD d, XINPUT_STATE *state)
{
    auto p = XInputGetState_fn();
    if(p != null) {
        return p(d, state);
    }
    return E_NOT_VALID_STATE;
}

};    // namespace XInput
