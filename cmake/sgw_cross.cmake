set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# See https://stackoverflow.com/questions/28613394/check-cmake-cache-variable-in-toolchain-file
# for why we need this dance back-and-forth with an env var....
# TLDR: CMake runs toolchain files multiple times, but can't read cache variables on some runs.
# -> on first run (in which cache variables are always accessible), set an intermediary environment variable
if (NOT DEFINED ENV{_toolhain_file_already_invoked})
    set(ENV{_toolhain_file_already_invoked} "1")

    #message(STATUS "***** FIRST INVOCATION *****")

    # We can't rely on the cache in toolchain files,
    # so we use environment variables to preserve everything we want to use
    # in the successive invocations of this toolchain file
    # since environment variables are always preserved
    set (ENV{_SYSROOT} "${SYSROOT}")
    set (ENV{_CROSS_TOOLCHAIN_ROOT} "${CROSS_TOOLCHAIN_ROOT}")
    set (ENV{_STAGING_FOLDER} "${STAGING_FOLDER}")
else()
    #message(STATUS "***** SUCCESSIVE INVOCATION *****")

    set (SYSROOT "$ENV{_SYSROOT}")
    set (CROSS_TOOLCHAIN_ROOT "$ENV{_CROSS_TOOLCHAIN_ROOT}")
    set (STAGING_FOLDER "$ENV{_STAGING_FOLDER}")
endif()


if (NOT CROSS_TOOLCHAIN_ROOT)
    message(FATAL_ERROR
        " ***** Pass -DCROSS_TOOLCHAIN_ROOT=</path/to/cross_toolchain_root> *****\n"
        " ***** e.g. -DCROSS_TOOLCHAIN_ROOT=/home/toolchain/gcc-linaro-6.3.1-2017.05-x86_64_arm-linux-gnueabihf *****")
endif()

if (NOT SYSROOT)
    message(FATAL_ERROR
        " ***** Pass -DSYSROOT=</path/to/sysroot> *****\n"
        " ***** e.g. -DSYSROOT=/home/SGW/rootfs *****")
endif()

set(CMAKE_SYSROOT "${SYSROOT}")

if (STAGING_FOLDER)
    set(CMAKE_STAGING_PREFIX "${STAGING_FOLDER}")
    message(STATUS "Will stage into ${CMAKE_STAGING_PREFIX}")
endif()


set(CMAKE_C_COMPILER ${CROSS_TOOLCHAIN_ROOT}/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${CROSS_TOOLCHAIN_ROOT}/bin/arm-linux-gnueabihf-g++)

# Uncomment this to stop cmake trying to perform linking on the test program
#set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Let secondary dependencies of libs found in CMAKE_SYSROOT be found
# accordingly
set(CMAKE_C_FLAGS_INIT "-Wl,-rpath-link -Wl,${CMAKE_SYSROOT}/usr/lib/arm-linux-gnueabihf")
set(CMAKE_CXX_FLAGS_INIT "-Wl,-rpath-link -Wl,${CMAKE_SYSROOT}/usr/lib/arm-linux-gnueabihf")

# Disable adding RPATH during build, as we're definitively not
# going to be able to run cross-compiled executables on the build host...
set(CMAKE_SKIP_BUILD_RPATH TRUE)

# Not needed as CMAKE_SYSROOT is also used to search
#set(CMAKE_FIND_ROOT_PATH "${SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

#set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR} "${SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig:${SYSROOT}/usr/share/pkgconfig")
#set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_FIND_ROOT_PATH})
