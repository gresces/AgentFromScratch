#!/usr/bin/env bash
set -euo pipefail

# ---- AgentFromScratch 一键安装脚本 -------------------------------------------
# 安装 Agent 核（需 root）和所有插件到系统目录。
#
# 用法：
#   ./install.sh              # 安装全部（核心 + 插件 + 配置目录）
#   ./install.sh core         # 仅安装核心
#   ./install.sh plugins      # 仅安装插件
#   ./install.sh config       # 仅创建配置目录
#
# 环境变量：
#   AFS_CONFIG_DIR  覆盖配置目录（默认 ${XDG_CONFIG_HOME:-~/.config}/afs）
#   PREFIX          覆盖核心安装前缀（默认 /usr/local）

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CORE_DIR="$SCRIPT_DIR/core"
PLUGIN_DIR_REPO="$SCRIPT_DIR/plugins"

# ---- 配置目录 ----------------------------------------------------------------
if [[ -z "${AFS_CONFIG_DIR:-}" ]]; then
    if [[ -n "${XDG_CONFIG_HOME:-}" ]]; then
        AFS_CONFIG_DIR="$XDG_CONFIG_HOME/afs"
    elif [[ -n "${HOME:-}" ]]; then
        AFS_CONFIG_DIR="$HOME/.config/afs"
    else
        echo "错误: 无法确定配置目录（请设置 HOME 或 AFS_CONFIG_DIR）"
        exit 1
    fi
fi

PREFIX="${PREFIX:-/usr/local}"

# ---- 颜色 --------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; }
header(){ echo -e "\n${BOLD}=== $* ===${NC}"; }

# ---- usage -------------------------------------------------------------------

usage() {
    echo "用法: $0 [core|plugins|config|all]"
    echo ""
    echo "  $0              # 安装全部（核心 + 插件 + 配置目录）"
    echo "  $0 core         # 仅安装核心 Agent（需 root 权限）"
    echo "  $0 plugins      # 仅编译并安装插件"
    echo "  $0 config       # 仅创建配置目录结构"
    echo ""
    echo "安装内容："
    echo "  core    → $PREFIX/bin/afs + $PREFIX/include/afs*.hh"
    echo "  plugins → $AFS_CONFIG_DIR/plugins/tool/"
    echo "  config  → $AFS_CONFIG_DIR/"
    echo ""
    echo "环境变量："
    echo "  AFS_CONFIG_DIR  配置目录（默认 \$XDG_CONFIG_HOME/afs）"
    echo "  PREFIX          核心安装前缀（默认 /usr/local）"
    exit 1
}

# ---- 权限检查 ----------------------------------------------------------------

ensure_root() {
    if [[ $EUID -ne 0 ]]; then
        error "核心安装需要 root 权限。"
        echo ""
        echo "  请使用以下方式之一："
        echo "    sudo $0 core"
        echo "    sudo $0"
        echo ""
        echo "  如果只需安装插件（无需 root）："
        echo "    $0 plugins"
        exit 1
    fi
}

ensure_not_root() {
    if [[ $EUID -eq 0 ]]; then
        # 以 root 安装插件会放到 /root/.config/afs/plugins/，通常不是期望行为
        warn "当前以 root 运行，插件将安装到 root 的配置目录。"
        warn "建议以普通用户身份运行插件安装。"
        echo ""
        read -rp "继续？[y/N] " yn
        case "$yn" in
            [Yy]*) ;;
            *)     exit 0 ;;
        esac
    fi
}

# ---- 依赖检查 ----------------------------------------------------------------

check_xmake() {
    if ! command -v xmake &>/dev/null; then
        error "未找到 xmake。请先安装 xmake："
        echo "  curl -fsSL https://xmake.io/shget.text | bash"
        echo "  或访问 https://xmake.io/#/getting_started"
        exit 1
    fi
}

# ---- 安装核心 ----------------------------------------------------------------

install_core() {
    header "安装 Agent 核"

    ensure_root
    check_xmake

    if [[ ! -d "$CORE_DIR" ]]; then
        error "未找到 core 目录：$CORE_DIR"
        exit 1
    fi

    # 确保没有 afs 进程在运行（否则安装会因 "file busy" 失败）
    if pkill afs 2>/dev/null; then
        info "已终止运行中的 afs 进程"
    fi

    cd "$CORE_DIR"

    # 构建 Release 版本
    info "编译 Agent 核 (Release) ..."
    xmake f -m release -y
    xmake

    # 安装
    info "安装到 $PREFIX ..."
    if [[ "$PREFIX" == "/usr/local" ]]; then
        xmake install --root
    else
        xmake install --root -o "$PREFIX"
    fi

    cd "$SCRIPT_DIR"

    info "核心安装完成："
    echo "  可执行文件：$PREFIX/bin/afs"
    echo "  公共头文件：$PREFIX/include/afs.hh"
    echo "  公共头文件：$PREFIX/include/afs/"
    echo ""
    echo "  验证：$PREFIX/bin/afs --help"
}

# ---- 安装插件 ----------------------------------------------------------------

install_plugins() {
    header "安装插件"

    ensure_not_root

    if [[ ! -f "$PLUGIN_DIR_REPO/build.sh" ]]; then
        error "未找到插件构建脚本：$PLUGIN_DIR_REPO/build.sh"
        exit 1
    fi

    info "编译并安装所有插件到 $AFS_CONFIG_DIR/plugins/ ..."
    cd "$PLUGIN_DIR_REPO"
    AFS_CONFIG_DIR="$AFS_CONFIG_DIR" ./build.sh install
    cd "$SCRIPT_DIR"
}

# ---- 配置目录 ----------------------------------------------------------------

setup_config() {
    header "创建配置目录"

    mkdir -p "$AFS_CONFIG_DIR"/plugins/tool
    mkdir -p "$AFS_CONFIG_DIR"/plugins/skill
    info "配置目录已创建：$AFS_CONFIG_DIR"
    echo "  $AFS_CONFIG_DIR/"
    echo "  ├── plugins/"
    echo "  │   ├── tool/"
    echo "  │   └── skill/"
    echo "  └── config.json    ← 在此放置你的模型配置"
}

# ---- 入口 --------------------------------------------------------------------

case "${1:-all}" in
    all)
        setup_config
        install_plugins
        echo ""
        info "核心安装需要 root 权限，请单独运行："
        echo "  sudo $0 core"
        ;;
    core)
        install_core
        ;;
    plugins)
        install_plugins
        ;;
    config)
        setup_config
        ;;
    -h|--help|help)
        usage
        ;;
    *)
        error "未知参数: $1"
        usage
        ;;
esac
