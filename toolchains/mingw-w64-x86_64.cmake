set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_CROSSCOMPILING_EMULATOR wine)

set(triple x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${triple}-gcc)
set(CMAKE_CXX_COMPILER ${triple}-g++)
set(CMAKE_RC_COMPILER  ${triple}-windres)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-O2)
add_link_options(-static-libgcc -static-libstdc++)

# Workaround: std::aligned_alloc is in C++17 but unavailable with mingw-w64.
# Windows CRT (MSVCRT) doesn't provide aligned_alloc, so libstdc++ can't
# expose std::aligned_alloc. We provide a shim using _aligned_malloc instead.
add_compile_options(-include "${CMAKE_CURRENT_LIST_DIR}/aligned_alloc.h")
