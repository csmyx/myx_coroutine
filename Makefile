.PHONY: all run config build build_lib build_main src format clean rebuild reconfig

all: build

BUILD_TYPE=Debug
config:
	cmake -B build -S . \
	-DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
	-DCMAKE_CXX_STANDARD=20 \
	-DVCPKG_TARGET_TRIPLET=x64-linux-libcxx

build: config
	cmake --build build -j

LIB_NAME = myx_coroutine
build_lib: config
	cmake --build build -j --target ${LIB_NAME}