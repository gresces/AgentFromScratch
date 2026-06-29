#!/usr/bin/env bash
set -euo pipefail

CORE="${CORE:-$(realpath "$(dirname "$0")/../../../core")}"
BIN="${BIN:-$(realpath "$(dirname "$0")/../../../bin")}"

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

NAME="bash"
TYPE="tool"
SRC="bash.cpp"

# 生成统一命名: ToolPluginBash 格式
capitalize() { echo "$1" | awk '{print toupper(substr($0,1,1)) substr($0,2)}'; }
OUT="$(capitalize "$TYPE")Plugin$(capitalize "$NAME")"

DEST="$PLUGIN_DIR/$TYPE"

CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--std=c++23 -fPIC -fvisibility=hidden -O2}"
INCLUDES="-I$CORE/include"

cmd="${1:-build}"

case "$cmd" in
    build)
        echo "  编译 $NAME → $OUT ..."
        $CXX $CXXFLAGS $INCLUDES -shared "$SRC" -o "$OUT"
        echo "  $OUT 已生成"
        ;;
    install)
        $0 build
        mkdir -p "$DEST"
        cp "$OUT" "$DEST/"
        echo "  已安装到 $DEST/$OUT"
        rm -f "$OUT"
        echo "  已清理本地构建产物 $OUT"
        ;;
    clean)
        rm -f "$OUT"
        echo "  已清理 $OUT"
        ;;
    *)
        echo "用法: $0 [build|install|clean]"
        exit 1
        ;;
esac
