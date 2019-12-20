//////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////////////////////////////////////////////////////

#if defined(UNICODE)
#error Unicode not supported
#endif

//////////////////////////////////////////////////////////////////////

#ifdef DLL_EXPORTS
#define DLL_API __declspec(dllexport)
#else
#define DLL_API __declspec(dllimport)
#endif

//////////////////////////////////////////////////////////////////////

constexpr nullptr_t null = nullptr;

using int8 = __int8;
using int16 = __int16;
using int32 = __int32;
using int64 = __int64;

using uint8 = unsigned __int8;
using uint16 = unsigned __int16;
using uint32 = unsigned __int32;
using uint64 = unsigned __int64;

using byte = uint8;

//////////////////////////////////////////////////////////////////////

extern DLL_API bool install_kbd_hook();
extern DLL_API void uninstall_kbd_hook();
extern DLL_API void log(char const *text, ...);
extern DLL_API void va_log(char const *text, va_list v);
extern DLL_API void log_raw(char const *text, int len);

//////////////////////////////////////////////////////////////////////

constexpr int pipe_buffer_size_bytes = 512;

DECLSPEC_SELECTANY char const *pipe_name = "\\\\.\\pipe\\fps_pipe_BA7AEFB1-6409-4D1E-8052-2444FC33A3A9";

DECLSPEC_SELECTANY char const *global_event_name = "Global\\fps_64_50FC63D0-ED19-442C-BBF1-5E29C31885C5";

#if defined(_WIN64)
DECLSPEC_SELECTANY char const *global_mutex = "Global\\fps_mutex_22AC5064-4CE4-4399-914C-7B6ABF4317C4";
#else
DECLSPEC_SELECTANY char const *global_mutex = "Global\\fps_mutex_94470FA0-3D9D-4546-A1E9-E3F71654D7D4";
#endif

//////////////////////////////////////////////////////////////////////
// mutex admin

inline bool already_running()
{
    // leak the handle, it will get closed when the process ends
    return CreateMutex(null, false, global_mutex) == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ALREADY_EXISTS;
}

//////////////////////////////////////////////////////////////////////

inline void debug_log(char const *f, ...)
{
    va_list v;
    va_start(v, f);
    char buffer[512];
    int len = vsnprintf_s(buffer, _countof(buffer) - 2, f, v);
    // assert(len > 0);
    buffer[len] = '\n';
    buffer[len + 1] = 0;
    OutputDebugString(buffer);
}
