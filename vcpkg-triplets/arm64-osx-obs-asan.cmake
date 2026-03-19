set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DCMAKE_INSTALL_DO_STRIP=OFF)
set(VCPKG_OSX_DEPLOYMENT_TARGET "12.0")
set(VCPKG_C_FLAGS "${VCPKG_C_FLAGS} -fsanitize=address")
set(VCPKG_CXX_FLAGS "${VCPKG_CXX_FLAGS} -fsanitize=address")
set(VCPKG_C_FLAGS_RELEASE "${VCPKG_C_FLAGS_RELEASE} -g -fno-omit-frame-pointer -fstack-protector-strong")
set(VCPKG_CXX_FLAGS_RELEASE "${VCPKG_CXX_FLAGS_RELEASE} -g -fno-omit-frame-pointer -fstack-protector-strong")
set(VCPKG_LINKER_FLAGS "${VCPKG_LINKER_FLAGS} -fsanitize=address")
