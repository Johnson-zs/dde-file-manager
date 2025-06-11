#!/bin/bash

# 简单的编译测试脚本
set -e

echo "Testing tests2 compilation..."

# 设置环境变量
export DFM_TEST_MODE=1

# 创建构建目录
BUILD_DIR="build-test"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 配置CMake
echo "Configuring CMake..."
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DOPT_ENABLE_BUILD_UT2=ON \
      ..

# 编译tests2
echo "Building tests2..."
make test-dfm-framework -j4

echo "Compilation test completed successfully!" 