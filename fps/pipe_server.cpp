//////////////////////////////////////////////////////////////////////
// copied from https://docs.microsoft.com/en-us/windows/win32/ipc/named-pipe-server-using-overlapped-i-o

#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include "..\fps_dll\fps_dll.h"
#include "..\fps_dll\defer.h"

//////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define FAIL(x, ...)               \
    do {                           \
        debug_log(x, __VA_ARGS__); \
        DebugBreak();              \
    } while(false)
#else
#define FAIL(...)
#endif

//////////////////////////////////////////////////////////////////////

extern HWND main_dlg;

namespace {
enum class pipe_state_t : int
{
    connecting = 0,
    reading = 1
};

//////////////////////////////////////////////////////////////////////

constexpr int pipe_count = 4;
constexpr int pipe_timeout = 5000;

//////////////////////////////////////////////////////////////////////

struct pipe_instance
{
    OVERLAPPED overlapped;
    HANDLE handle = INVALID_HANDLE_VALUE;
    char buffer[pipe_buffer_size_bytes + 1];    // +1 to force null termination when we send to text control
    DWORD bytes_read;
    pipe_state_t state;
    bool pending_io;

    //////////////////////////////////////////////////////////////////////

    bool new_client()
    {
        if(handle != INVALID_HANDLE_VALUE) {
            if(!DisconnectNamedPipe(handle)) {
                FAIL("DisconnectNamedPipe failed (%08x)", GetLastError());
                return false;
            }
        }
        DWORD flags = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
        DWORD type = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;
        DWORD size = pipe_buffer_size_bytes;
        handle = CreateNamedPipe(pipe_name, flags, type, pipe_count, size, size, pipe_timeout, NULL);

        if(handle == INVALID_HANDLE_VALUE) {
            FAIL("CreateNamedPipe failed (%08x)", GetLastError());
            return false;
        }

        pending_io = false;

        if(ConnectNamedPipe(handle, &overlapped) != 0) {
            FAIL("ConnectNamedPipe failed (%08x)", GetLastError());
            return false;
        }

        switch(GetLastError()) {

        case ERROR_IO_PENDING:
            pending_io = true;
            state = pipe_state_t::connecting;
            break;

        case ERROR_PIPE_CONNECTED:
            SetEvent(overlapped.hEvent);
            state = pipe_state_t::reading;
            break;

        default: {
            FAIL("ConnectNamedPipe failed (%08x)", GetLastError());
            return false;
        }
        }
        return true;
    }

    //////////////////////////////////////////////////////////////////////

    bool handle_event()
    {
        if(pending_io) {
            DWORD cbRet;
            // could be waiting on ReadFile or ConnectNamedPipe
            bool ok = GetOverlappedResult(handle, &overlapped, &cbRet, false);
            switch(state) {
            case pipe_state_t::connecting:
                if(!ok) {
                    FAIL("GetOverlappedResult failed (%08x)", GetLastError());
                    return false;
                }
                state = pipe_state_t::reading;
                break;

            case pipe_state_t::reading: {
                if(!ok || cbRet == 0) {
                    return new_client();
                }
                bytes_read = cbRet;
                char *temp_buf = new char[bytes_read + 1];
                memcpy(temp_buf, buffer, bytes_read);
                temp_buf[bytes_read] = 0;
                PostMessage(main_dlg, WM_USER, bytes_read, (LPARAM)temp_buf);
                pending_io = false;
            } break;

            default: {
                FAIL("Invalid pipe state");
                return false;
            }
            }
        }

        if(state != pipe_state_t::reading) {
            FAIL("Invalid pipe state");
            return false;
        }
        bool ok = ReadFile(handle, buffer, pipe_buffer_size_bytes, &bytes_read, &overlapped);
        if(!ok && GetLastError() != ERROR_IO_PENDING) {

            DWORD e = GetLastError();
            return new_client();
        }
        pending_io = true;
        return true;
    }
};

//////////////////////////////////////////////////////////////////////

pipe_instance pipe[pipe_count];
HANDLE event_handles[pipe_count + 1];

}    // namespace

//////////////////////////////////////////////////////////////////////

DWORD WINAPI pipe_server(void *exit_event_arg)
{
    event_handles[0] = (HANDLE)exit_event_arg;

    for(int i = 1; i <= pipe_count; i++) {

        event_handles[i] = CreateEvent(NULL, true, true, NULL);

        if(event_handles[i] == NULL) {
            DWORD e = GetLastError();
            FAIL("CreateEvent failed (%08x)", GetLastError());
            return 0;
        }
        pipe[i - 1].overlapped.hEvent = event_handles[i];
        pipe[i - 1].new_client();
    }

    debug_log("pipe_server is ready");
    extern HANDLE pipe_thread_start_event;
    SetEvent(pipe_thread_start_event);

    while(true) {

        DWORD i = WaitForMultipleObjects(pipe_count + 1, event_handles, false, INFINITE);

        if(i < 0 || i > pipe_count) {
            FAIL("WaitForMultipleObjects returned index %d out of range", i);
            break;
        }

        if(i == 0) {    // WAIT_OBJECT_0 = quit_event
            break;
        }

        if(!pipe[i - 1].handle_event()) {
            break;
        }
    }
    return 0;
}
