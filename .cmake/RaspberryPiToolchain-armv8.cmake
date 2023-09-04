include("/build/x-tools/armv8-rpi3-linux-gnueabihf/armv8-rpi3-linux-gnueabihf.toolchain.cmake")
set(CROSS_COMPILE_SYSROOT /build/x-tools/armv8-rpi3-linux-gnueabihf/armv8-rpi3-linux-gnueabihf/sysroot)
set(CROSS_COMPILE_PKG_CONFIG_PATH "${CROSS_COMPILE_SYSROOT}/usr/lib/arm-linux-gnueabi/pkgconfig")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
