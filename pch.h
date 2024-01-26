// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include "framework.h"

extern "C" _declspec(dllexport) void init_dxgi(HWND hwnd);
extern "C" _declspec(dllexport) BYTE * grab(BYTE * buffer, int left, int top, int right, int bottom);
extern "C" _declspec(dllexport) void destroy();


#endif //PCH_H
