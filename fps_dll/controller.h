#pragma once

namespace XInput
{
bool is_connected();
void cleanup();
DWORD WINAPI GetState(DWORD d, XINPUT_STATE *state);
}
