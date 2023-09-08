include("/build/x-tools/armv6-rpi-linux-gnueabihf/armv6-rpi-linux-gnueabihf.toolchain.cmake")
set(CROSS_COMPILE_SYSROOT /build/x-tools/armv6-rpi-linux-gnueabihf/armv6-rpi-linux-gnueabihf/sysroot)
set(CROSS_COMPILE_PKG_CONFIG_PATH "${CROSS_COMPILE_SYSROOT}/usr/lib/arm-linux-gnueabi/pkgconfig")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
