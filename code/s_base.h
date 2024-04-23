#if !defined(S_BASE_H)
#define S_BASE_H

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

#define function static
#define global static
#define local static

#define unused(var) (void)var
#define null 0
#define array_count(a) (sizeof(a)/(sizeof((a)[0])))
#define memory_copy(dst,src,sz) memcpy(dst,src,sz)

#define _stringify(s) #s
#define stringify(s) _stringify(s)

#define debug_break() __debugbreak()

#if defined(S_DEBUG)
#define s_assert(cond,msg) \
	if (!(cond)) {\
		debug_break();\
	}
#else
#define s_assert(cond,msg)
#endif

typedef struct {
	u8 *str;
	u64 char_count;
	u64 char_capacity;
} String_Const_U8;

typedef String_Const_U8 String_U8;

function String_Const_U8
str8_make(char *c, u64 length);

#define str8(s) str8_make(s,sizeof(s)-1)

#endif
