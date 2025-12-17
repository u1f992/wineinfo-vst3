#pragma once

#if defined(__cplusplus) && defined(__MINGW32__)
#include <malloc.h>
namespace std {
inline void *aligned_alloc(size_t alignment, size_t size) {
  return _aligned_malloc(size, alignment);
}
}
#endif
