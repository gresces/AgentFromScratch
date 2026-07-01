# plugins > AGENTS-CN.md

插件源码目录，包含所有插件的源代码和顶层构建脚本。

## 文件

| 文件 | 职责 |
|------|------|
| `build.sh` | 顶层编译脚本：自动发现 `context/*/build.sh`、`loop/*/build.sh`、`tools/*/build.sh`、`skills/*/build.sh` 并批量编译 |
| `context/` | Context 类型插件源码目录 |
| `loop/` | Loop 类型插件源码目录 |
| `tools/` | 工具插件源码目录 |

## 使用方法

```sh
cd plugins

./build.sh              # 编译全部插件
./build.sh install      # 安装全部到 ${XDG_CONFIG_HOME:-~/.config}/afs/plugins/<type>/
./build.sh clean        # 清理全部

./build.sh compute             # 仅编译 compute 插件
./build.sh compute install     # 编译并安装 compute
./build.sh file install        # 编译并安装 file
./build.sh context/simple      # 仅编译 Context 运行时插件
./build.sh loop/simple install # 编译并安装 Loop 运行时插件
```

环境变量可覆盖默认值：
```sh
CXX=clang++ CXXFLAGS="-std=c++23 -O3" ./build.sh
CORE=/path/to/core AFS_CONFIG_DIR=/path/to/config/afs ./build.sh install
PLUGIN_DIR=/path/to/afs/plugins ./build.sh compute install
```

## 编译流程

```
./build.sh
  │
  ├── 确定 CORE 路径 (默认 ../core)
  ├── 确定 BIN  路径 (默认 ../bin)
  ├── 确定 PLUGIN_DIR 路径 (默认 ${XDG_CONFIG_HOME:-~/.config}/afs/plugins)
  ├── 遍历 context/*/build.sh、loop/*/build.sh、tools/*/build.sh、skills/*/build.sh
  │     └── 每个: cd 到插件目录, bash build.sh
  │           ├── 编译 → <Type>Plugin<Name>
  │           └── install → 复制到 $PLUGIN_DIR/<type>/
  └── 完成
```

## 命名规则

编译产物由各插件的 `build.sh` 按 `<Type>Plugin<Name>` 格式自动生成：

```
TYPE=tool, NAME=compute  →  ToolPluginCompute
TYPE=skill, NAME=search  →  SkillPluginSearch
TYPE=context, NAME=simple →  ContextPluginSimple
TYPE=loop, NAME=simple    →  LoopPluginSimple
```

## 目录结构

```
plugins/
├── build.sh              ← 顶层脚本（自动发现）
├── AGENTS-CN.md
├── context/
│   └── simple/
│       ├── build.sh
│       ├── xmake.lua
│       ├── context.cpp
│       └── AGENTS-CN.md
├── loop/
│   └── simple/
│       ├── build.sh
│       ├── xmake.lua
│       ├── loop.cpp
│       └── AGENTS-CN.md
└── tools/
    └── <name>/
        ├── build.sh       ← 单插件编译脚本
        ├── <name>.cpp     ← 插件源码
        └── AGENTS-CN.md   ← 插件文档（参考 tools/file/AGENTS-CN.md）

${XDG_CONFIG_HOME:-~/.config}/afs/
└── plugins/
    ├── tool/
    │   ├── ToolPluginCompute
    │   ├── ToolPluginFile
    │   └── ToolPluginWeather
    ├── skill/
    │   └── ...
    ├── context/
    │   └── ContextPluginSimple
    └── loop/
        └── LoopPluginSimple
```

## 添加新插件

1. 选择类型目录：Context 插件放 `plugins/context/<name>`，Loop 插件放 `plugins/loop/<name>`，工具插件放 `plugins/tools/<name>`
2. 创建 `build.sh`，必须支持 `build`、`install`、`clean`
3. 按需创建 `xmake.lua`（Context/Loop 运行时插件使用 xmake 包管理依赖）
4. 创建插件源码和 `AGENTS-CN.md`
5. `cd plugins && ./build.sh my_plugin` 测试编译
6. 顶层脚本自动发现，无需修改
