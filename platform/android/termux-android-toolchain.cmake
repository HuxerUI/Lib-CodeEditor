# Termux cross-toolchain for Android.
#
# Termux runs an Android userspace, so CMake reports CMAKE_HOST_SYSTEM_NAME
# "Android" and the NDK toolchain cannot resolve its host tag. Use Termux's
# native aarch64 clang/lld with the NDK sysroot instead, targeting the ABI
# passed by the Android Gradle plugin.
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 23)
set(ANDROID_PLATFORM android-23)
if(NOT DEFINED ANDROID_ABI)
  set(ANDROID_ABI arm64-v8a)
endif()
set(CMAKE_ANDROID_ARCH_ABI "${ANDROID_ABI}")
# CMake's Android module refuses Android hosts when CMAKE_ANDROID_NDK is set;
# hide it (everything below is provided explicitly).
set(_TERMUX_NDK_ROOT "${CMAKE_ANDROID_NDK}")
unset(CMAKE_ANDROID_NDK CACHE)
if(NOT _TERMUX_NDK_ROOT)
  set(_TERMUX_NDK_ROOT "$ENV{ANDROID_NDK_HOME}")
endif()
set(_NDK_TOOLCHAIN_ROOT "${_TERMUX_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64")
if(ANDROID_ABI STREQUAL "arm64-v8a")
  set(CMAKE_SYSTEM_PROCESSOR aarch64)
  set(_ANDROID_TARGET aarch64-linux-android23)
  set(_ANDROID_TRIPLE aarch64-linux-android)
elseif(ANDROID_ABI STREQUAL "x86_64")
  set(CMAKE_SYSTEM_PROCESSOR x86_64)
  set(_ANDROID_TARGET x86_64-linux-android23)
  set(_ANDROID_TRIPLE x86_64-linux-android)
else()
  message(FATAL_ERROR "Unsupported ABI for Termux toolchain: ${ANDROID_ABI}")
endif()
set(CMAKE_C_COMPILER /data/data/com.termux/files/usr/bin/clang)
set(CMAKE_CXX_COMPILER /data/data/com.termux/files/usr/bin/clang++)
set(CMAKE_ASM_COMPILER /data/data/com.termux/files/usr/bin/clang)
set(CMAKE_AR /data/data/com.termux/files/usr/bin/llvm-ar CACHE FILEPATH "llvm-ar")
set(CMAKE_RANLIB /data/data/com.termux/files/usr/bin/llvm-ranlib CACHE FILEPATH "llvm-ranlib")
set(CMAKE_STRIP /data/data/com.termux/files/usr/bin/llvm-strip CACHE FILEPATH "llvm-strip")
set(CMAKE_OBJCOPY /data/data/com.termux/files/usr/bin/llvm-objcopy CACHE FILEPATH "llvm-objcopy")
set(CMAKE_SYSROOT "${_NDK_TOOLCHAIN_ROOT}/sysroot")
# Size-optimized builds: -Oz and function/data sections with gc-sections to
# drop dead code. Unwind tables and LTO are deliberately NOT disabled: the
# editor's event handling relies on C++ exceptions (try/catch around core
# gestures/keys), and -fno-unwind-tables breaks stack unwinding so those
# handlers silently swallow exceptions (events stop responding). LTO is left
# off because the prebuilt third-party static libs (oniguruma/simdutf) have no
# LTO bitcode and thin-LTO interop is a risk.
set(_HUXERUI_SIZE_FLAGS "-Oz -ffunction-sections -fdata-sections")
set(_HUXERUI_SIZE_LINK_FLAGS "-Wl,--gc-sections -Wl,--strip-all")
set(CMAKE_C_FLAGS_INIT "-target ${_ANDROID_TARGET} --sysroot=${CMAKE_SYSROOT} -fPIC ${_HUXERUI_SIZE_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "-target ${_ANDROID_TARGET} --sysroot=${CMAKE_SYSROOT} -fPIC ${_HUXERUI_SIZE_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-target ${_ANDROID_TARGET} --sysroot=${CMAKE_SYSROOT} -fuse-ld=lld ${_HUXERUI_SIZE_LINK_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-target ${_ANDROID_TARGET} --sysroot=${CMAKE_SYSROOT} -fuse-ld=lld ${_HUXERUI_SIZE_LINK_FLAGS}")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# Point find_library at the NDK per-ABI library dirs (CMake's Android module is
# bypassed on an Android host, so it cannot contribute its own search paths).
set(CMAKE_SYSTEM_LIBRARY_PATH
  "${CMAKE_SYSROOT}/usr/lib/${_ANDROID_TRIPLE}/23"
  "${CMAKE_SYSROOT}/usr/lib/${_ANDROID_TRIPLE}"
)
