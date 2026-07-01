#!/usr/bin/env bash
set -euo pipefail

CORE="${CORE:-$(realpath "$(dirname "$0")/../../../core")}" 

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

NAME="simple"
TYPE="loop"
OUT="LoopPluginSimple"
DEST="$PLUGIN_DIR/$TYPE"

cmd="${1:-build}"

case "$cmd" in
    build)
        echo "  xmake 编译 $NAME → $OUT ..."
        CORE="$CORE" xmake -y
        echo "  $OUT 已生成"
        ;;
    install)
        "$0" build
        mkdir -p "$DEST"
        cp "$OUT" "$DEST/"
        echo "  已安装到 $DEST/$OUT"
        ;;
    clean)
        xmake clean
        rm -f "$OUT"
        echo "  已清理 $OUT"
        ;;
    *)
        echo "用法: $0 [build|install|clean]"
        exit 1
        ;;
esac
