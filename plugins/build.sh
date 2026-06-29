#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORE="${CORE:-$(realpath "$SCRIPT_DIR/../core")}"
BIN="${BIN:-$(realpath "$SCRIPT_DIR/../bin")}"

CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--std=c++23 -fPIC -fvisibility=hidden -O2}"
INCLUDES="-I$CORE/include"

usage() {
    echo "用法: $0 [all|install|clean] [plugin_name]"
    echo "  $0              # 编译全部插件"
    echo "  $0 install      # 安装全部插件到 bin/plugins/"
    echo "  $0 clean        # 清理全部插件"
    echo "  $0 compute      # 编译指定插件"
    exit 1
}

build_one() {
    local name="$1"
    local dir="$SCRIPT_DIR/tools/$name"
    local build_sh="$dir/build.sh"
    if [[ ! -f "$build_sh" ]]; then
        echo "错误: 未找到 $name 插件 ($build_sh)"
        return 1
    fi
    echo "=== 编译 $name ==="
    cd "$dir"
    CORE="$CORE" BIN="$BIN" bash "$build_sh" "${@:2}"
    cd "$SCRIPT_DIR"
}

build_all() {
    local cmd="${1:-build}"
    local found=0
    for dir in "$SCRIPT_DIR"/tools/*/; do
        local name=$(basename "$dir")
        if [[ -f "$dir/build.sh" ]]; then
            build_one "$name" "$cmd"
            found=1
        fi
    done
    if [[ $found -eq 0 ]]; then
        echo "未发现任何插件 (tools/*/build.sh)"
    fi
}

case "${1:-}" in
    ""|all)
        build_all
        ;;
    install)
        build_all install
        ;;
    clean)
        build_all clean
        ;;
    *)
        if [[ -d "$SCRIPT_DIR/tools/$1" ]]; then
            build_one "$1" "${2:-build}"
        else
            usage
        fi
        ;;
esac
