#include <stdio.h>
#include <stdint.h>

typedef uint8_t   u8;
typedef  int8_t   s8;
typedef uint16_t u16;
typedef  int16_t s16;
typedef uint32_t u32;
typedef  int32_t s32;
typedef uint64_t u64;
typedef  int64_t s64;
typedef float f32;
typedef double f64;
typedef s32 b32;

enum {
    False,
    True
};

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define NOMINMAX
#include <windows.h>

#define unused(var) (void)var

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
        PSTR lpCmdLine, int nCmdShow) {
    unused(hInstance);
    unused(hPrevInstance);
    unused(lpCmdLine);
    unused(nCmdShow);
    OutputDebugStringA("\n\nHello Wrld\n\n");
    return 0;
}