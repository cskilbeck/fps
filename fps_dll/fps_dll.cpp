//////////////////////////////////////////////////////////////////////
// TODO (chs): make it work with titles which use RawInput (keyboard hook doesn't get called) by subclassing the title window and intercepting a custom message to toggle the hook

#include <utility>
#include <stdio.h>
#include <Windows.h>
#include <xinput.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <d3d9.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <wrl.h>
#include <mmsystem.h>
#include "minhook/include/MinHook.h"
#include "kiero.h"
#include "fps_dll.h"
#include "defer.h"
#include "controller.h"

#pragma comment(lib, "winmm.lib")

#if defined(_DEBUG)
#if defined(_WIN64)
#pragma comment(lib, "..\\fps_dll\\minhook\\build\\VC15\\x64\\Debug\\libMinHook_x64.lib")
#else
#pragma comment(lib, "..\\fps_dll\\minhook\\build\\VC15\\Win32\\Debug\\libMinHook_x86.lib")
#endif
#else
#if defined(_WIN64)
#pragma comment(lib, "..\\fps_dll\\minhook\\build\\VC15\\x64\\Release\\libMinHook_x64.lib")
#else
#pragma comment(lib, "..\\fps_dll\\minhook\\build\\VC15\\Win32\\Release\\libMinHook_x86.lib")
#endif
#endif

#ifdef _WIN64
char const *cpu_platform = "x64";
#else
char const *cpu_platform = "x86";
#endif

//////////////////////////////////////////////////////////////////////
// shared data section

#pragma data_seg(".shared")

HHOOK keyboard_hook_handle = null;    // keyboard hook handle

#pragma data_seg()

#pragma comment(linker, "/SECTION:.shared,RWS")

//////////////////////////////////////////////////////////////////////

namespace {

// local (instanced) data section

HANDLE pipe_handle = INVALID_HANDLE_VALUE;
char pipe_buffer[pipe_buffer_size_bytes];

UINT tilde_keycode = VK_OEM_3;
HMODULE hooked_process = null;                      // which process is hooked
HWND hooked_window = null;                          // HWND of the hooked window
HINSTANCE dll_handle;                               // current DLL handle
CRITICAL_SECTION log_critical_section;              // critsec for logging
INIT_ONCE log_init_once = INIT_ONCE_STATIC_INIT;    // initonce for logging critsec

// size of flashing rectangle
int const rect_width = 120;
int const rect_height = 120;
RECT const fill_rect{ 0, 0, rect_width, rect_height };

// fillrect colors (RGB)
uint32_t const error_no_dll_color{ 0x00ffff };           // CYAN - DLL not found
uint32_t const error_no_controller_color{ 0xff0000 };    // RED - controller disconnected
uint32_t const error_unknown_error_color{ 0xffff00 };    // YELLOW - unknown controller error
uint32_t const idle_color{ 0x000000 };                   // BLACK - A button released
uint32_t const flash_color{ 0xffffff };                  // WHITE - A button pressed
uint32_t fill_color{ idle_color };                       // Current one to fillrect with

int last_press_time = 0;                   // for debouncing the hotkey reader
int hotkey_debounce_threshold_ms = 250;    // min time between presses

//////////////////////////////////////////////////////////////////////
// d3d9 overrides

typedef long(__stdcall *D3D9_Present)(LPDIRECT3DDEVICE9, const RECT *, const RECT *, HWND, const RGNDATA *);

D3D9_Present d3d9_old_present = null;
bool d3d9_mode_shown = false;

//////////////////////////////////////////////////////////////////////
// d3d11 overrides

typedef long(__stdcall *DXGI_Present)(IDXGISwapChain *swap_chain, UINT interval, UINT mode);

DXGI_Present dxgi_old_present = null;

template <typename T> using com_ptr = Microsoft::WRL::ComPtr<T>;

com_ptr<ID3D11Device1> d3d11_device;
com_ptr<ID3D11DeviceContext1> d3d11_device_context;
com_ptr<IDXGISwapChain1> dxgi_swapchain1;
com_ptr<ID3D11Device1> d3d11_device1;
com_ptr<ID3D11DeviceContext1> d3d11_device_context1;
com_ptr<ID3D11RenderTargetView> d3d11_render_target_view;

bool dxgi_mode_shown = false;

//////////////////////////////////////////////////////////////////////
// crud for logging

struct flag_name
{
    char const *name;
    uint32_t flag;
};

struct value_name
{
    char const *name;
    uint32_t value;
};

char const *dxgi_swap_effect_names[] = {
    "DXGI_SWAP_EFFECT_DISCARD",            // = 0,
    "DXGI_SWAP_EFFECT_SEQUENTIAL",         // = 1,
    "DXGI_SWAP_EFFECT_UNKNOWN",            // ! 2
    "DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL",    // = 3,
    "DXGI_SWAP_EFFECT_FLIP_DISCARD"        // = 4
};

flag_name const dxgi_swap_chain_flags[] = {

    { "DXGI_SWAP_CHAIN_FLAG_NONPREROTATED", DXGI_SWAP_CHAIN_FLAG_NONPREROTATED },
    { "DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH", DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH },
    { "DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE", DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE },
    { "DXGI_SWAP_CHAIN_FLAG_RESTRICTED_CONTENT", DXGI_SWAP_CHAIN_FLAG_RESTRICTED_CONTENT },
    { "DXGI_SWAP_CHAIN_FLAG_RESTRICT_SHARED_RESOURCE_DRIVER", DXGI_SWAP_CHAIN_FLAG_RESTRICT_SHARED_RESOURCE_DRIVER },
    { "DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY", DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY },
    { "DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT", DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT },
    { "DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER", DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER },
    { "DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO", DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO },
    { "DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO", DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO },
    { "DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED", DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED },
    { "DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING", DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING },
    { "DXGI_SWAP_CHAIN_FLAG_RESTRICTED_TO_ALL_HOLOGRAPHIC_DISPLAYS", DXGI_SWAP_CHAIN_FLAG_RESTRICTED_TO_ALL_HOLOGRAPHIC_DISPLAYS }
};

flag_name const dxgi_present_flags[] = { { "DXGI_PRESENT_TEST", DXGI_PRESENT_TEST },
                                         { "DXGI_PRESENT_DO_NOT_SEQUENCE", DXGI_PRESENT_DO_NOT_SEQUENCE },
                                         { "DXGI_PRESENT_RESTART", DXGI_PRESENT_RESTART },
                                         { "DXGI_PRESENT_DO_NOT_WAIT", DXGI_PRESENT_DO_NOT_WAIT },
                                         { "DXGI_PRESENT_STEREO_PREFER_RIGHT", DXGI_PRESENT_STEREO_PREFER_RIGHT },
                                         { "DXGI_PRESENT_STEREO_TEMPORARY_MONO", DXGI_PRESENT_STEREO_TEMPORARY_MONO },
                                         { "DXGI_PRESENT_RESTRICT_TO_OUTPUT", DXGI_PRESENT_RESTRICT_TO_OUTPUT },
                                         { "DXGI_PRESENT_USE_DURATION", DXGI_PRESENT_USE_DURATION },
                                         { "DXGI_PRESENT_ALLOW_TEARING", DXGI_PRESENT_ALLOW_TEARING } };

flag_name const d3d9_present_flags[] = { { "D3DPRESENTFLAG_LOCKABLE_BACKBUFFER", D3DPRESENTFLAG_LOCKABLE_BACKBUFFER },
                                         { "D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL", D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL },
                                         { "D3DPRESENTFLAG_DEVICECLIP", D3DPRESENTFLAG_DEVICECLIP },
                                         { "D3DPRESENTFLAG_VIDEO", D3DPRESENTFLAG_VIDEO },
                                         { "D3DPRESENTFLAG_NOAUTOROTATE", D3DPRESENTFLAG_NOAUTOROTATE },
                                         { "D3DPRESENTFLAG_UNPRUNEDMODE", D3DPRESENTFLAG_UNPRUNEDMODE },
                                         { "D3DPRESENTFLAG_OVERLAY_LIMITEDRGB", D3DPRESENTFLAG_OVERLAY_LIMITEDRGB },
                                         { "D3DPRESENTFLAG_OVERLAY_YCbCr_BT709", D3DPRESENTFLAG_OVERLAY_YCbCr_BT709 },
                                         { "D3DPRESENTFLAG_OVERLAY_YCbCr_xvYCC", D3DPRESENTFLAG_OVERLAY_YCbCr_xvYCC },
                                         { "D3DPRESENTFLAG_RESTRICTED_CONTENT", D3DPRESENTFLAG_RESTRICTED_CONTENT },
                                         { "D3DPRESENTFLAG_RESTRICT_SHARED_RESOURCE_DRIVER", D3DPRESENTFLAG_RESTRICT_SHARED_RESOURCE_DRIVER } };

char const *d3d9_swap_effect_names[] = {
    "D3DSWAPEFFECT_UNKNOWN",    // 0
    "D3DSWAPEFFECT_DISCARD",    // 1
    "D3DSWAPEFFECT_FLIP",       // 2
    "D3DSWAPEFFECT_COPY",       // 3
    "D3DSWAPEFFECT_OVERLAY"     // 4
    "D3DSWAPEFFECT_FLIPEX",     // 5
};

value_name const d3d9_interval_names[] = { { "D3DPRESENT_INTERVAL_DEFAULT", D3DPRESENT_INTERVAL_DEFAULT }, { "D3DPRESENT_INTERVAL_ONE", D3DPRESENT_INTERVAL_ONE },
                                           { "D3DPRESENT_INTERVAL_TWO", D3DPRESENT_INTERVAL_TWO },         { "D3DPRESENT_INTERVAL_THREE", D3DPRESENT_INTERVAL_THREE },
                                           { "D3DPRESENT_INTERVAL_FOUR", D3DPRESENT_INTERVAL_FOUR },       { "D3DPRESENT_INTERVAL_IMMEDIATE", D3DPRESENT_INTERVAL_IMMEDIATE } };

// frame timing admin

double clock_frequency;
double previous_timestamp;
double first_timestamp;
int frame_counter = 0;

//////////////////////////////////////////////////////////////////////

double get_timestamp()
{
    uint64_t timestamp;
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&timestamp));
    return static_cast<double>(timestamp);
}

//////////////////////////////////////////////////////////////////////

#if 0
void record_frame_time()
{
    // do this for first 10 frames (rather than just 1) to allow the framerate to settle after hook installed
    if(++frame_counter < 10) {
        uint64_t frequency;
        QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER *>(&frequency));
        clock_frequency = static_cast<double>(frequency);
        previous_timestamp = get_timestamp();
        first_timestamp = previous_timestamp;
        timings.num_timings = 0;
    } else {
        double time_now = get_timestamp();
        double time_delta = time_now - previous_timestamp;
        previous_timestamp = time_now;
        if(timings.num_timings < _countof(timings.timings)) {
            timings.timings[timings.num_timings] = time_delta;
            timings.num_timings += 1;
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////

void log_summary()
{
    //    log("Frames: %d", timings.num_timings);
    double total_time = (previous_timestamp - first_timestamp) / clock_frequency;
    log("Total time: %f", total_time);
    //    log("Average framerate: %f", 1.0 / (total_time / timings.num_timings));
    frame_counter = 0;
}

//////////////////////////////////////////////////////////////////////

BOOL CALLBACK InitCriticalSectionFN(PINIT_ONCE init_once, PVOID, PVOID *)
{
    InitializeCriticalSection(&log_critical_section);
    return true;
}

//////////////////////////////////////////////////////////////////////

void log_flags(uint32_t flags, flag_name const flag_names[], size_t num_flag_names)
{
    if(flags == 0) {
        log("    None!");
        return;
    }
    for(size_t n = 0; n != num_flag_names; ++n) {
        auto const &f = flag_names[n];
        if((flags & f.flag) != 0) {
            log("    %s", f.name);
        }
    }
}

//////////////////////////////////////////////////////////////////////

char const *get_value_name(uint32_t value, value_name const value_names[], size_t num_value_names)
{
    for(size_t n = 0; n != num_value_names; ++n) {
        auto const &v = value_names[n];
        if(value == v.value) {
            return v.name;
        }
    }
    return "Unknown";
}

//////////////////////////////////////////////////////////////////////

void warn(bool &warned, char const *message, ...)
{
    if(warned == false) {
        warned = true;    // not threadsafe
        va_list v;
        va_start(v, message);
        va_log(message, v);
    }
}

//////////////////////////////////////////////////////////////////////
// Present() draw a rectangle black or white depending on joypad button

long __stdcall d3d9_present(LPDIRECT3DDEVICE9 pDevice, const RECT *r, const RECT *s, HWND h, const RGNDATA *d)
{
    if(d3d9_mode_shown == false) {
        d3d9_mode_shown = true;
        IDirect3DSwapChain9 *swap_chain = null;
        D3DDISPLAYMODE display_mode;
        D3DPRESENT_PARAMETERS present_parameters;
        HRESULT hr = pDevice->GetSwapChain(0, &swap_chain);
        if(FAILED(hr)) {
            log("Error: can't get Swap Chain: %08x", hr);
        } else {
            hr = swap_chain->GetDisplayMode(&display_mode);
            if(FAILED(hr)) {
                log("Error: can't GetDisplayMode: %08x", hr);
            } else {
                hr = swap_chain->GetPresentParameters(&present_parameters);
                if(FAILED(hr)) {
                    log("Error: can't GetPresentParameters: %08x", hr);
                } else {
                    char const *swap_effect_name = d3d9_swap_effect_names[0];
                    if(present_parameters.SwapEffect < _countof(dxgi_swap_effect_names)) {
                        swap_effect_name = d3d9_swap_effect_names[present_parameters.SwapEffect];
                    }
                    log("\r\nD3D9::Present()");
                    log("DisplayMode    : %ux%u (%uHz)", display_mode.Width, display_mode.Height, display_mode.RefreshRate);
                    log("SwapChain      : %ux%u", present_parameters.BackBufferWidth, present_parameters.BackBufferHeight);
                    log("BackBufferCount: %u", present_parameters.BackBufferCount);
                    log("Swap Effect    : %s", swap_effect_name);
                    log("Windowed       : %s", present_parameters.Windowed ? "True" : "False");
                    log("Interval       : %s", get_value_name(present_parameters.PresentationInterval, d3d9_interval_names, _countof(d3d9_interval_names)));
                    log("Refresh Rate   : %u", present_parameters.FullScreen_RefreshRateInHz);
                    log("Flags:");
                    log_flags(present_parameters.Flags, d3d9_present_flags, _countof(d3d9_present_flags));
                    log("\r\n");
                }
                swap_chain->Release();
            }
        }
    }
    // record_frame_time();

    static bool warned = false;
    HRESULT hr = pDevice->Clear(1, reinterpret_cast<D3DRECT const *>(&fill_rect), D3DCLEAR_TARGET, fill_color, 0, 0);    // ARGB
    if(FAILED(hr)) {
        warn(warned, "pDevice->Clear failed: %08x", hr);
    }

    pDevice->BeginScene();
    pDevice->EndScene();

    return d3d9_old_present(pDevice, r, s, h, d);
}

//////////////////////////////////////////////////////////////////////

bool init_d3d11(IDXGISwapChain *swap_chain)
{
    static bool warned = false;

    if(d3d11_device_context1 == null) {

        HRESULT hr = swap_chain->QueryInterface<IDXGISwapChain1>(&dxgi_swapchain1);
        if(FAILED(hr)) {
            warn(warned, "swap_chain->QueryInterface failed: %08x", hr);
            return false;
        }

        hr = dxgi_swapchain1->GetDevice(__uuidof(ID3D11Device1), (void **)&d3d11_device1);
        if(FAILED(hr)) {
            warn(warned, "dxgi_swapchain1->GetDevice failed: %08x", hr);
            return false;
        }

        com_ptr<ID3D11Texture2D> back_buffer;
        hr = dxgi_swapchain1->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&back_buffer);
        if(FAILED(hr)) {
            warn(warned, "dxgi_swapchain1->GetBuffer failed: %08x", hr);
            return false;
        }

        hr = d3d11_device1->CreateRenderTargetView(back_buffer.Get(), NULL, &d3d11_render_target_view);
        back_buffer.Reset();
        if(FAILED(hr)) {
            warn(warned, "d3d11_device1->CreateRenderTargetView failed: %08x", hr);
            return false;
        }
        d3d11_device1->GetImmediateContext1(&d3d11_device_context1);
    }
    return d3d11_device_context1 != null;
}

//////////////////////////////////////////////////////////////////////

void cleanup_d3d11()
{
    if(d3d11_device_context1 != null) {
        log("Cleanup D3D11");
        d3d11_device_context1.Reset();
        d3d11_render_target_view.Reset();
        d3d11_device1.Reset();
        dxgi_swapchain1.Reset();
    }
}

//////////////////////////////////////////////////////////////////////

void uint32_to_floats(uint32_t color, float f[4])
{
    f[3] = 1;
    for(int i = 0; i < 3; ++i) {
        f[i] = ((color >> 16) & 0xff) / 255.0f;
        color <<= 8;
    }
}

//////////////////////////////////////////////////////////////////////

long __stdcall dxgi_present(IDXGISwapChain *swap_chain, UINT interval, UINT flags)
{
    // show some details about the swap chain
    if(dxgi_mode_shown == false) {    // not thread safe
        dxgi_mode_shown = true;
        DXGI_SWAP_CHAIN_DESC swap_chain_desc;
        if(SUCCEEDED(swap_chain->GetDesc(&swap_chain_desc))) {
            float numerator = static_cast<float>(swap_chain_desc.BufferDesc.RefreshRate.Numerator);
            float denominator = static_cast<float>(swap_chain_desc.BufferDesc.RefreshRate.Denominator);
            float refresh_rate = 0;
            if(numerator == 0) {
                if(denominator == 1) {
                    refresh_rate = 1;
                }
            } else {
                refresh_rate = numerator / denominator;
            }
            char const *swap_effect_name = dxgi_swap_effect_names[2];
            if(swap_chain_desc.SwapEffect < _countof(dxgi_swap_effect_names)) {
                swap_effect_name = dxgi_swap_effect_names[swap_chain_desc.SwapEffect];
            }
            log("\r\nPresent()");
            log("Interval        : %d", interval);
            log("SwapChain       : %dx%d", swap_chain_desc.BufferDesc.Width, swap_chain_desc.BufferDesc.Height);
            log("Refresh rate    : %f", refresh_rate);
            log("Swap Effect     : %s", swap_effect_name);
            log("Buffer Count    : %d", swap_chain_desc.BufferCount);
            log("Windowed        : %s", swap_chain_desc.Windowed ? "True" : "False");
            log("Swap Chain Flags:");
            log_flags(swap_chain_desc.Flags, dxgi_swap_chain_flags, _countof(dxgi_swap_chain_flags));
            log("Present Flags:");
            log_flags(flags, dxgi_present_flags, _countof(dxgi_present_flags));
            log("\r\n");
        }
    }
    // record_frame_time();

    if(init_d3d11(swap_chain)) {

        float clear_color[4];
        uint32_to_floats(fill_color, clear_color);
        d3d11_device_context1->ClearView(d3d11_render_target_view.Get(), clear_color, reinterpret_cast<D3D11_RECT const *>(&fill_rect), 1);
    }

    return dxgi_old_present(swap_chain, interval, flags);
}

//////////////////////////////////////////////////////////////////////

bool setup_hook()
{
    kiero::Status status = kiero::open();
    if(status != kiero::Status::Success) {
        log("Failed to init kiero: %d", status);
        return false;
    }
    log("Kiero init OK");

    hooked_process = GetModuleHandle(null);

    log("Adding D3D9 hook");
    MH_STATUS bind_status = kiero::bind(kiero::DXType::dx9, 17, (void **)&d3d9_old_present, d3d9_present);
    if(bind_status != MH_OK) {
        log("Error binding D3D9::Present: %08x (%s)", bind_status, MH_StatusToString(bind_status));
    } else {
        log("D3D9 bind() complete");
    }

    log("Adding D3D11 hook");
    bind_status = kiero::bind(kiero::DXType::dx11, 8, (void **)&dxgi_old_present, dxgi_present);
    if(bind_status != MH_OK) {
        log("Error binding DXGI::Present: %08x (%s)", bind_status, MH_StatusToString(bind_status));
    } else {
        log("D3D11 bind() complete");
    }
    return true;
}

//////////////////////////////////////////////////////////////////////

void hook_removed()
{
    hooked_process = null;
    hooked_window = null;
    dxgi_mode_shown = false;
    d3d9_mode_shown = false;
}

//////////////////////////////////////////////////////////////////////

void remove_hook()
{
    log("remove_hook() called");
    log_summary();
    cleanup_d3d11();
    kiero::close();
    hook_removed();
}

//////////////////////////////////////////////////////////////////////

LRESULT CALLBACK kbd_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if(code >= 0) {
        int vk_code = (int)wparam;

        if(vk_code == tilde_keycode) {
            bool previous_state = (lparam & (1 << 30)) != 0;
            bool alt = (lparam & (1 << 29)) != 0;
            int repeat_count = lparam & 0x7fff;
            bool transition = (lparam & (1U << 31)) != 0;

            // sometimes you get two of the same in a row very fast, not sure why, just ignore the 2nd one
            int cur_press_time = timeGetTime();
            int press_diff_time = cur_press_time - last_press_time;
            last_press_time = cur_press_time;

            // hotkey is BACKTICK
            if(press_diff_time > hotkey_debounce_threshold_ms && previous_state == false) {    // VK_OEM_3 = 0xC0 = backtick

                log("Hotkey pressed: transition: %d", transition);

                HWND foreground_window = GetForegroundWindow();
                if(foreground_window != null) {
                    log("Window: %p", foreground_window);
                    if(hooked_window == null) {
                        if(setup_hook()) {
                            hooked_window = foreground_window;
                        } else {
                            log("Failed to bind - wrong D3D?");
                        }
                    } else {
                        log("Already hooked, so...");
                        if(foreground_window == hooked_window) {
                            log("Removing hook");
                            remove_hook();
                            hooked_window = null;
                        } else if(hooked_window != null) {
                            log("Can't unhook: different application!");
                        } else {
                            log("Huh?");
                        }
                    }
                }
            }
        }
    }
    return CallNextHookEx(keyboard_hook_handle, code, wparam, lparam);
}

}    // namespace

//////////////////////////////////////////////////////////////////////

void log_raw(char const *text, int len)
{
    InitOnceExecuteOnce(&log_init_once, InitCriticalSectionFN, null, null);
    EnterCriticalSection(&log_critical_section);
    defer(LeaveCriticalSection(&log_critical_section));
    if(pipe_handle == INVALID_HANDLE_VALUE) {
        pipe_handle = CreateFile(pipe_name, GENERIC_WRITE, 0, null, OPEN_EXISTING, 0, null);
        if(pipe_handle == INVALID_HANDLE_VALUE) {
            debug_log("Run out of pipes!? catastrophe, increase the limit");
            return;
        }
    }
    while(len > 0) {
        int put = len;
        if(put > pipe_buffer_size_bytes) {
            put = pipe_buffer_size_bytes;
        }
        DWORD wrote;
        if(!WriteFile(pipe_handle, text, put, &wrote, null)) {
            return;
        }
        len -= wrote;
        text += wrote;
    }
}

//////////////////////////////////////////////////////////////////////

void va_log(char const *text, va_list v)
{
    char buffer[4096];
    int len = _vsnprintf_s(buffer, _countof(buffer) - 3, text, v);
    if(len < 0) {
        return;
    }
    char *c = buffer + len;
    if(*c != '\r' && *c != '\n') {
        *c++ = '\r';
        *c++ = '\n';
        *c++ = 0;
        len += 3;
    }
    log_raw(buffer, len);
}

//////////////////////////////////////////////////////////////////////

void log(char const *text, ...)
{
    va_list v;
    va_start(v, text);
    va_log(text, v);
}

//////////////////////////////////////////////////////////////////////

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reason, LPVOID lpReserved)
{
    switch(reason) {

    case DLL_PROCESS_ATTACH: {
        dll_handle = (HINSTANCE)hModule;
        DisableThreadLibraryCalls(dll_handle);
        char module_name[MAX_PATH];
        GetModuleFileName(NULL, module_name, _countof(module_name));
        HANDLE exe_handle = GetModuleHandle(null);
        // log("DLL Loaded: Process %p", exe_handle);
        // log("%s", module_name);
    } break;

    case DLL_PROCESS_DETACH: {
        log("Process detach");
        HANDLE exe_handle = GetModuleHandle(null);
        if(exe_handle == hooked_process) {
            hook_removed();
        }
    } break;
    }
    return TRUE;
}

//////////////////////////////////////////////////////////////////////

bool install_kbd_hook()
{
    // find the VK_ keycode for the tilde key
    char layout_name[KL_NAMELENGTH];
    if(!GetKeyboardLayoutName(layout_name)) {
        strncpy_s(layout_name, "UNKNOWN?", _countof(layout_name));
    }
    HKL keyboard_layout = GetKeyboardLayout(GetCurrentThreadId());
    if(keyboard_layout == null) {
        log("Can't GetKeyboardLayout: GetLastError: %08x", GetLastError());
    } else {
        tilde_keycode = MapVirtualKeyEx(0x29, MAPVK_VSC_TO_VK, keyboard_layout);
        if(tilde_keycode == 0) {
            log("Can't get VK code for tilde, defaulting to VK_OEM_3 (%d)", VK_OEM_3);
            tilde_keycode = VK_OEM_3;
        } else {
            debug_log("VK_ code for tilde key is %d (layout %s)", tilde_keycode, layout_name);
        }
    }

    keyboard_hook_handle = SetWindowsHookEx(WH_KEYBOARD, (HOOKPROC)kbd_hook_proc, dll_handle, 0);
    log("Keyboard hook for %s is %p", cpu_platform, keyboard_hook_handle);
    return keyboard_hook_handle != null;
}

//////////////////////////////////////////////////////////////////////

void uninstall_kbd_hook()
{
    remove_hook();

    if(keyboard_hook_handle != null) {
        log("Removing kbd hook");
        UnhookWindowsHookEx(keyboard_hook_handle);
        keyboard_hook_handle = null;
    }
}
