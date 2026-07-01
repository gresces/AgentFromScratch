#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORE="${CORE:-$(realpath "$SCRIPT_DIR/../core")}"
BIN="${BIN:-$(realpath "$SCRIPT_DIR/../bin")}"

if [[ -z "${AFS_CONFIG_DIR:-}" ]]; then
    if [[ -n "${XDG_CONFIG_HOME:-}" ]]; then
        AFS_CONFIG_DIR="$XDG_CONFIG_HOME/afs"
    elif [[ -n "${HOME:-}" ]]; then
        AFS_CONFIG_DIR="$HOME/.config/afs"
    else
        AFS_CONFIG_DIR="$PWD/.config/afs"
    fi
fi
PLUGIN_DIR="${PLUGIN_DIR:-$AFS_CONFIG_DIR/plugins}"

CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--std=c++23 -fPIC -fvisibility=hidden -O2}"
INCLUDES="-I$CORE/include"
PLUGIN_ROOTS=("context" "loop" "tools" "skills")


usage() {
    echo "用法: $0 [all|install|clean|plugin_name|type/plugin_name] [build|install|clean]"
    echo "  $0                        # 编译全部插件"
    echo "  $0 install                # 编译并安装全部插件到 $PLUGIN_DIR"
    echo "  $0 clean                  # 清理全部插件"
    echo "  $0 compute                # 编译唯一同名插件"
    echo "  $0 context/simple         # 编译指定类型插件"
    echo "  $0 loop/simple install    # 编译并安装指定类型插件"
    exit 1
}

find_plugin_dir() {
    local name="$1"
    if [[ "$name" == */* ]]; then
        local typed_dir="$SCRIPT_DIR/$name"
        [[ -d "$typed_dir" ]] && echo "$typed_dir" && return 0
        return 1
    fi

    local matched=""
    for root in "${PLUGIN_ROOTS[@]}"; do
        local dir="$SCRIPT_DIR/$root/$name"
        if [[ -d "$dir" ]]; then
            if [[ -n "$matched" ]]; then
                echo "错误: 插件名 $name 不唯一,请使用 <type>/$name" >&2
                return 2
            fi
            matched="$dir"
        fi
    done
    [[ -n "$matched" ]] && echo "$matched" && return 0
    return 1
}

build_one() {
    local name="$1"
    local dir
    if ! dir="$(find_plugin_dir "$name")"; then
        echo "错误: 未找到 $name 插件"
        return 1
    fi
    local build_sh="$dir/build.sh"
    if [[ ! -f "$build_sh" ]]; then
        echo "错误: 未找到 $name 插件构建脚本 ($build_sh)"
        return 1
    fi
    echo "=== $name ==="
    cd "$dir"
    CORE="$CORE" BIN="$BIN" AFS_CONFIG_DIR="$AFS_CONFIG_DIR" PLUGIN_DIR="$PLUGIN_DIR" \
        bash "$build_sh" "${@:2}"
    cd "$SCRIPT_DIR"
}

build_all() {
    local cmd="${1:-build}"
    local found=0
    for root in "${PLUGIN_ROOTS[@]}"; do
        for dir in "$SCRIPT_DIR"/"$root"/*/; do
            [[ -d "$dir" ]] || continue
            local name
            name=$(basename "$dir")
            if [[ -f "$dir/build.sh" ]]; then
                build_one "$root/$name" "$cmd"
                found=1
            fi
        done
    done
    if [[ $found -eq 0 ]]; then
        echo "未发现任何插件 ({context,loop,tools,skills}/*/build.sh)"
        return
    fi
    if [[ "$cmd" == "install" ]]; then
        echo ""
        echo "=== 安装完成 ==="
        echo "插件已安装到 $PLUGIN_DIR"
        ls -la "$PLUGIN_DIR"/tool/ 2>/dev/null || true
        ls -la "$PLUGIN_DIR"/skill/ 2>/dev/null || true
        ls -la "$PLUGIN_DIR"/context/ 2>/dev/null || true
        ls -la "$PLUGIN_DIR"/loop/ 2>/dev/null || true
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
        if find_plugin_dir "$1" >/dev/null; then
            build_one "$1" "${2:-build}"
        else
            usage
        fi
        ;;
esac
