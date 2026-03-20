#pragma once

#include <stdlib.h>

#define MALLOC_CAP_SPIRAM  0x00000200
#define MALLOC_CAP_8BIT    0x00000004
#define MALLOC_CAP_DEFAULT 0x00000000

static inline void* heap_caps_calloc(size_t n, size_t size, uint32_t caps)
{
    (void)caps;
    return calloc(n, size);
}

static inline void heap_caps_free(void* ptr)
{
    free(ptr);
}
