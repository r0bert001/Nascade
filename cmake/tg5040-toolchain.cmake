# TrimUI tg5040 / Brick Pro 官方 SDK 交叉编译工具链
# 用法:
#   cmake -S . -B build/tg5040 -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/tg5040-toolchain.cmake \
#     -DNASCADE_CROSS=ON \
#     -DNASCADE_SYSROOT=/opt/tg5040/sysroot \
#     -DNASCADE_DEPS=/opt/tg5040/deps ...

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TG5040_TOOLCHAIN "/opt/tg5040/toolchain" CACHE PATH "TrimUI Linaro toolchain root")
set(TG5040_SYSROOT "/opt/tg5040/sysroot" CACHE PATH "TrimUI SDK userland sysroot")
set(TG5040_DEPS "/opt/tg5040/deps" CACHE PATH "Nascade cross-built dependencies")

set(CMAKE_C_COMPILER "${TG5040_TOOLCHAIN}/bin/aarch64-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${TG5040_TOOLCHAIN}/bin/aarch64-linux-gnu-g++")
set(CMAKE_AR "${TG5040_TOOLCHAIN}/bin/aarch64-linux-gnu-ar")
set(CMAKE_RANLIB "${TG5040_TOOLCHAIN}/bin/aarch64-linux-gnu-ranlib")

# 不要覆盖编译器自带的 libc sysroot（toolchain/aarch64-linux-gnu/libc）。
# SDK sysroot 只作为额外的头文件/运行库搜索路径。
set(CMAKE_FIND_ROOT_PATH "${TG5040_SYSROOT}" "${TG5040_DEPS}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS_INIT "-O2 -I${TG5040_SYSROOT}/usr/include")
set(CMAKE_CXX_FLAGS_INIT "-O2 -I${TG5040_SYSROOT}/usr/include")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-L${TG5040_SYSROOT}/usr/lib -L${TG5040_DEPS}/lib -Wl,-rpath-link,${TG5040_SYSROOT}/usr/lib -Wl,-rpath-link,${TG5040_DEPS}/lib")
