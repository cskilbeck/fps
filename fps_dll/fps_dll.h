#pragma once

#if defined(UNICODE)
#error Unicode not supported
#endif

#ifdef DLL_EXPORTS
#define DLL_API __declspec(dllexport)
#else
#define DLL_API __declspec(dllimport)
#endif

constexpr nullptr_t null = nullptr;

extern DLL_API bool install_kbd_hook(HWND dlg);
extern DLL_API void uninstall_kbd_hook();
extern DLL_API void log(char const *text, ...);
extern DLL_API void va_log(char const *text, va_list v);
extern DLL_API void log_raw(char const *text);
extern DLL_API void setup_buttons(uint32_t mask);
extern DLL_API uint32_t get_button_mask();

struct frame_timings
{
    int num_timings;
    double timings[60 * 60 * 60];
};
