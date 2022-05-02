#!/bin/bash

export ARCH=arm64
mkdir out
BUILD_CROSS_COMPILE=/workspace/v4.14-274-a71/toolchain/gcc/bin/aarch64-linux-android-
KERNEL_LLVM_BIN=/workspace/v4.14-274-a71/toolchain/clang/bin/clang
CLANG_TRIPLE=aarch64-linux-gnu-
KERNEL_MAKE_ENV="DTC_EXT=$(pwd)/tools/dtc CONFIG_BUILD_ARM64_DT_OVERLAY=y"

make -j16 -C $(pwd) O=$(pwd)/out $KERNEL_MAKE_ENV ARCH=arm64 CROSS_COMPILE=$BUILD_CROSS_COMPILE REAL_CC=$KERNEL_LLVM_BIN CLANG_TRIPLE=$CLANG_TRIPLE a71_defconfig
PATH="/workspace/v4.14-274-a71/toolchain/clang/bin:$PATH" \
make -j16 -C $(pwd) O=$(pwd)/out $KERNEL_MAKE_ENV ARCH=arm64 LD=ld.lld CROSS_COMPILE=$BUILD_CROSS_COMPILE REAL_CC=$KERNEL_LLVM_BIN CLANG_TRIPLE=$CLANG_TRIPLE
 
cp out/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image
