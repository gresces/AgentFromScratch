#!/usr/bin/env bash
set -euo pipefail

CORE="${CORE:-$(realpath "$(dirname "$0")/../../../core")}"
BIN="${BIN:-$(realpath "$(dirname "$0")/../../../bin")}"

NAME="compute"
TYPE="tool"
SRC="compute.cpp"

# 生成统一命名: ToolPluginCompute 格式
capitalize() { echo "$1" | awk '{print toupper(substr($0,1,1)) substr($0,2)}'; }
OUT="$(capitalize "$TYPE")Plugin$(capitalize "$NAME")"

DEST="$BIN/plugins/$TYPE"

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
