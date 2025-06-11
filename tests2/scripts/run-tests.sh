#!/bin/bash

# DDE File Manager Tests2 运行脚本
# 用于编译和运行tests2项目的所有测试

set -e  # 遇到错误立即退出

# 脚本配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TESTS2_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$TESTS2_DIR")"
BUILD_DIR="$TESTS2_DIR/build"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 显示帮助信息
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help          显示帮助信息"
    echo "  -c, --clean         清理构建目录"
    echo "  -r, --rebuild       重新构建（清理后构建）"
    echo "  -t, --test-only     只运行测试，不重新编译"
    echo "  -v, --verbose       详细输出"
    echo "  --coverage          生成覆盖率报告"
    echo "  --standalone        独立编译模式"
    echo ""
    echo "Examples:"
    echo "  $0                  # 编译并运行测试"
    echo "  $0 --clean          # 清理构建目录"
    echo "  $0 --rebuild        # 重新构建并运行测试"
    echo "  $0 --coverage       # 运行测试并生成覆盖率报告"
}

# 解析命令行参数
CLEAN=false
REBUILD=false
TEST_ONLY=false
VERBOSE=false
COVERAGE=false
STANDALONE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -r|--rebuild)
            REBUILD=true
            shift
            ;;
        -t|--test-only)
            TEST_ONLY=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        --coverage)
            COVERAGE=true
            shift
            ;;
        --standalone)
            STANDALONE=true
            shift
            ;;
        *)
            log_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
done

# 检查环境
check_environment() {
    log_info "检查环境..."
    
    # 检查必要的工具
    local tools=("cmake" "make" "g++")
    for tool in "${tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            log_error "$tool 未找到，请安装后重试"
            exit 1
        fi
    done
    
    # 检查Qt6
    if ! pkg-config --exists Qt6Core; then
        log_error "Qt6 未找到，请安装Qt6开发包"
        exit 1
    fi
    
    # 检查GTest
    if ! pkg-config --exists gtest; then
        log_warning "GTest pkg-config未找到，尝试查找系统安装"
    fi
    
    # 设置测试模式环境变量
    export DFM_TEST_MODE=1
    log_info "设置测试模式: DFM_TEST_MODE=$DFM_TEST_MODE"
    
    log_success "环境检查完成"
}

# 清理构建目录
clean_build() {
    log_info "清理构建目录..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        log_success "构建目录已清理"
    else
        log_info "构建目录不存在，无需清理"
    fi
}

# 配置CMake
configure_cmake() {
    log_info "配置CMake..."
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    local cmake_args=(
        "-DCMAKE_BUILD_TYPE=Debug"
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    )
    
    if [ "$VERBOSE" = true ]; then
        cmake_args+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
    fi
    
    if [ "$COVERAGE" = true ]; then
        cmake_args+=("-DCMAKE_CXX_FLAGS=-fprofile-arcs -ftest-coverage")
        cmake_args+=("-DCMAKE_C_FLAGS=-fprofile-arcs -ftest-coverage")
    fi
    
    log_info "CMake参数: ${cmake_args[*]}"
    
    if [ "$STANDALONE" = true ]; then
        # 独立编译模式
        cmake "${cmake_args[@]}" "$TESTS2_DIR"
    else
        # 作为子项目编译模式
        cmake "${cmake_args[@]}" "$PROJECT_ROOT"
    fi
    
    log_success "CMake配置完成"
}

# 编译项目
build_project() {
    log_info "编译项目..."
    
    cd "$BUILD_DIR"
    
    local make_args=("-j$(nproc)")
    if [ "$VERBOSE" = true ]; then
        make_args+=("VERBOSE=1")
    fi
    
    make "${make_args[@]}"
    
    log_success "编译完成"
}

# 运行测试
run_tests() {
    log_info "运行测试..."
    
    cd "$BUILD_DIR"
    
    # 设置测试环境变量
    export DFM_TEST_MODE=1
    
    # 运行ctest
    local ctest_args=("--output-on-failure")
    if [ "$VERBOSE" = true ]; then
        ctest_args+=("--verbose")
    fi
    
    if ctest "${ctest_args[@]}"; then
        log_success "所有测试通过"
        return 0
    else
        log_error "测试失败"
        return 1
    fi
}

# 生成覆盖率报告
generate_coverage() {
    if [ "$COVERAGE" != true ]; then
        return 0
    fi
    
    log_info "生成覆盖率报告..."
    
    cd "$BUILD_DIR"
    
    # 检查lcov是否可用
    if ! command -v lcov &> /dev/null; then
        log_warning "lcov未找到，跳过覆盖率报告生成"
        return 0
    fi
    
    # 生成覆盖率数据
    lcov --capture --directory . --output-file coverage.info
    
    # 过滤不需要的文件
    lcov --remove coverage.info \
        '/usr/*' \
        '*/tests2/*' \
        '*/3rdparty/*' \
        '*/build/*' \
        '*moc_*' \
        '*qrc_*' \
        --output-file coverage_filtered.info
    
    # 生成HTML报告
    if command -v genhtml &> /dev/null; then
        genhtml coverage_filtered.info --output-directory coverage_html
        log_success "覆盖率报告生成完成: $BUILD_DIR/coverage_html/index.html"
    else
        log_warning "genhtml未找到，无法生成HTML报告"
    fi
}

# 主函数
main() {
    log_info "开始运行DDE File Manager Tests2"
    log_info "项目根目录: $PROJECT_ROOT"
    log_info "Tests2目录: $TESTS2_DIR"
    log_info "构建目录: $BUILD_DIR"
    
    # 检查环境
    check_environment
    
    # 清理构建目录（如果需要）
    if [ "$CLEAN" = true ] || [ "$REBUILD" = true ]; then
        clean_build
    fi
    
    # 如果只运行测试，检查构建目录是否存在
    if [ "$TEST_ONLY" = true ]; then
        if [ ! -d "$BUILD_DIR" ] || [ ! -f "$BUILD_DIR/Makefile" ]; then
            log_error "构建目录不存在或未配置，请先运行完整构建"
            exit 1
        fi
    else
        # 配置和编译
        configure_cmake
        build_project
    fi
    
    # 运行测试
    if run_tests; then
        # 生成覆盖率报告
        generate_coverage
        log_success "测试运行完成"
    else
        log_error "测试运行失败"
        exit 1
    fi
}

# 运行主函数
main "$@" 