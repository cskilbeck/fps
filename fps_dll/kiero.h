//////////////////////////////////////////////////////////////////////
// https://github.com/Rebzzel/kiero/blob/master/LICENSE
//
// MIT License
//
// Copyright (c) 2014-2019 Rebzzel
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

//////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////

#if defined(_M_X64)
using uint_ptr = uint64_t;
#else
using uint_ptr = uint32_t;
#endif

//////////////////////////////////////////////////////////////////////

namespace kiero {

enum Status
{
    Success = 0,
    UnknownError = -1,
    NotSupportedError = -2,
    ModuleNotFoundError = -3,
    AllocFailed = -4,
    CreateDeviceFailed = -5,
    FunctionNotFound = -6,
    GetDisplayModeFailed = -7,
    CreateD3DFailed = -8,
    CreateWindowFailed = -9,
    RegisterClassFailed = -10,
    InitHookerFailed = -11
};

enum DXType : int
{
    dx9 = 0,
    dx11 = 1,
    dx_num_types = 2
};

Status open();
void close();

MH_STATUS bind(DXType dx_type, uint16_t index, void **original, void *function);

}    // namespace kiero
