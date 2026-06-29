# plugins > tools > AGENTS-CN.md

工具插件源码目录，每个子目录为一个独立插件。

## 插件列表

| 目录 | 功能 |
|------|------|
| `bash/` | 通过 `/bin/bash -lc` 执行 Shell 命令并返回输出与退出码 |
| `compute/` | 二元加减乘除运算 |

## 约定

- 每个插件子目录必须有 `build.sh`，支持 `build`、`install`、`clean` 子命令。
- 编译产物命名 `<Type>Plugin<Name>`，无后缀，安装到 `bin/plugins/<type>/`。
- 插件源码仅依赖 `core/include/` 公共头文件。
