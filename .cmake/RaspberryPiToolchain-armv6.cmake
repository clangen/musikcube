# set(CMAKE_SYSTEM_NAME Linux)
# set(CMAKE_SYSTEM_PROCESSOR armv6)

# set(CROSS_COMPILE_SYSROOT /build/rpi-armv6/sysroot)
# set(CROSS_COMPILE_PKG_CONFIG_PATH "${CROSS_COMPILE_SYSROOT}/usr/lib/arm-linux-gnueabi/pkgconfig")

# set(CMAKE_SYSROOT ${CROSS_COMPILE_SYSROOT})

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv6 -marm")
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv6 -marm")

# set(CMAKE_C_COMPILER arm-linux-gnueabi-gcc)
# set(CMAKE_CXX_COMPILER arm-linux-gnueabi-g++)

# set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

include("/build/x-tools/armv6-rpi-linux-gnueabihf/armv6-rpi-linux-gnueabihf.toolchain.cmake")
