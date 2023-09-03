# set(CMAKE_SYSTEM_NAME Linux)
# set(CMAKE_SYSTEM_PROCESSOR armv7-a)

# set(CROSS_COMPILE_SYSROOT /build/rpi-armv7a/sysroot)
# set(CROSS_COMPILE_PKG_CONFIG_PATH "${CROSS_COMPILE_SYSROOT}/usr/lib/arm-linux-gnueabihf/pkgconfig")

# set(CMAKE_SYSROOT ${CROSS_COMPILE_SYSROOT})

# set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
# set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

include("/build/x-tools/armv8-rpi3-linux-gnueabihf/armv8-rpi3-linux-gnueabihf.toolchain.cmake")
