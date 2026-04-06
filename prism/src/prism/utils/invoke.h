#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef uintptr_t (*InvokeFunc)();
uintptr_t invoke(uintptr_t (*native_code)(), uintptr_t* args, size_t length);
#ifdef __cplusplus
}
#endif