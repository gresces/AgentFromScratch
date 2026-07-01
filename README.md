# AgentFromScratch

A high-performance, extensible personal Coding Agent built in C++23 — runs as a standalone terminal binary.

## Quick Start

### Build

```sh
cd core
xmake
# → bin/afs
```

Requires [xmake](https://xmake.io) v3+ and a C++23 compiler (gcc 14+ / clang 18+).

### Run

```sh
./bin/afs                              # TUI mode (default config)
./bin/afs config.json                  # TUI with explicit config
./bin/afs config.json "What is 2+2?"   # Console mode, single Q&A
```

Default config path: `${XDG_CONFIG_HOME:-~/.config}/afs/config.json`.

### Install

```sh
# One-shot: plugins + config dir (no root)
./install.sh

# Core binary (needs root for /usr/local)
sudo ./install.sh core

# Everything at once
sudo ./install.sh
```

Config template (`~/.config/afs/config.json`):

```json
{
  "models": {
    "llms": [
      {
        "name": "DeepSeek",
        "base_url": "https://api.deepseek.com",
        "api_key": "sk-xxxx",
        "model": "deepseek-v4-pro"
      }
    ],
    "default_llm": "DeepSeek"
  }
}
```

## Features

- **TUI interface** built with FTXUI — status bar, scrollable messages, sidebar (quick index + file browser), draggable splitter.
- **Agent / Shell modes** — toggle with `Tab`. Shell commands execute via `/bin/bash -lc` in your working directory.
- **`@` file completion** — type `@` followed by a path prefix; `↑`/`↓` to navigate candidates, `Tab` to complete.
- **Plugin system** — load runtime and tool plugins at startup. Context and Loop are separate runtime plugin types; tools are callable by the Agent.
- **OpenAI-compatible** — works with any OpenAI-compatible API (DeepSeek, local LLMs, etc.).
- **Tree-structured agents** — parent agents own child sub-agents via `unique_ptr`; recursive tool inheritance.

## Built-in Plugins

| Type | Plugin | Capability | Description |
|------|--------|------------|-------------|
| `context` | `simple` | `AFS::Context` | Message history and token accounting |
| `loop` | `simple` | `AFS::Loop` | LLM/tool execution loop |
| `tool` | `compute` | `compute` | Binary arithmetic |
| `tool` | `bash` | `execute_bash` | Shell command execution |
| `tool` | `file` | `file_read`, `file_write`, `file_exists` | File I/O (1 MiB read cap) |

Plugins are compiled separately and installed to `${XDG_CONFIG_HOME:-~/.config}/afs/plugins/<type>/`.

## Build Plugins

```sh
cd plugins
./build.sh              # build all
./build.sh install      # build + install
./build.sh context/simple      # build single Context runtime plugin
./build.sh loop/simple install # build + install single Loop runtime plugin
./build.sh file         # build single tool plugin
./build.sh file install # build + install single tool plugin
```

## Project Structure

```
AgentFromScratch/
├── core/                     # Agent core (xmake)
│   ├── include/              #   Public API headers (plugin dev entry point)
│   └── src/                  #   Source (main, agent, loop, config, models, TUI, plugins)
├── plugins/                  # Plugin source (independent compilation)
│   ├── context/              #   Context runtime plugin
│   ├── loop/                 #   Loop runtime plugin
│   └── tools/                #   Tool plugins (compute, bash, file, …)
├── bin/                      # Build output (afs binary)
├── install.sh                # One-shot install script
├── AGENTS-CN.md              # Full Chinese documentation
├── DEV-STATUS.md             # Development status log
└── TODO.md                   # Task backlog
```

## Writing a Plugin

```cpp
#include <afs.hh>

class MyPlugin final : public AFS::Plugin {
public:
    const char* name() const override { return "my_tool"; }
    AFS::PluginType type() const override { return AFS::PluginType::Tool; }
    void start() override {}
    void stop() override {}

    std::vector<ToolCap> toolCapabilities() const override {
        return {{"my_tool", "Does something useful",
                 R"({"type":"object","properties":{"x":{"type":"number"}}})",
                 [](const std::string& input) -> std::string {
                     return R"({"result":42})";
                 }}};
    }
};

AFS_PLUGIN_EXPORT std::uint32_t pluginAbiVersion() { return AFS::PluginAbiVersion; }
AFS_PLUGIN_EXPORT AFS::Plugin* createPlugin() { return new MyPlugin(); }
AFS_PLUGIN_EXPORT void destroyPlugin(AFS::Plugin* p) { delete p; }
```

Compile as a shared library:

```sh
c++ -std=c++23 -fPIC -shared my_plugin.cpp -I core/include -o ToolPluginMyTool
cp ToolPluginMyTool ~/.config/afs/plugins/tool/
```

See `plugins/tools/compute/` for a complete example with `build.sh`.

## Dependencies

| Library | Purpose |
|---------|---------|
| [nlohmann/json](https://github.com/nlohmann/json) | JSON parsing |
| [FTXUI](https://github.com/ArthurSonzogni/FTXUI) | Terminal UI |
| [Boost.SML](https://github.com/boost-ext/sml) | Agent state machine |
| [Taskflow](https://github.com/taskflow/taskflow) | Task parallelism |
| [cpr](https://github.com/libcpr/cpr) | HTTP client |

All managed via xmake/xrepo.

## Documentation

See **[AGENTS-CN.md](AGENTS-CN.md)** for comprehensive Chinese documentation covering architecture, conventions, and API reference.
