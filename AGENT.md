## 项目概述

Godot 4.3+ SSE (Server-Sent Events) Client GDExtension。使用 C++17 实现，通过 godot-cpp 绑定为 Godot 原生节点类 `SSEClient`，供 GDScript 调用，用于与 AI Agent（OpenAI 兼容 API 等）进行 SSE 流式通信。

## 技术栈

- **C++17**（`-std=c++17`）
- **godot-cpp 4.3-stable**（Git submodule，路径 `godot-cpp/`）
- **SCons** 构建系统
- **Godot 内置 HTTPClient** 处理 HTTP/TLS，零外部 C++ 依赖
- **doctest v2.4+** C++ 单元测试（单头文件，仅测试时使用）
- **Python 3 标准库** Mock SSE 服务器（零第三方依赖）

## 构建命令

```bash
# 首次克隆后初始化子模块
git submodule update --init --recursive

# 构建（当前平台）
scons platform=linux target=template_debug      # Linux debug
scons platform=linux target=template_release     # Linux release
scons platform=windows target=template_debug     # Windows debug
scons platform=windows target=template_release   # Windows release
scons platform=macos target=template_debug       # macOS debug
scons platform=macos target=template_release     # macOS release

# 输出路径: demo/bin/libsse_client.<platform>.<target>.<arch>.<ext>
```

## 测试命令

```bash
# C++ 单元测试（SSEParser，脱离 Godot 独立运行）
cd tests/cpp && make run

# 启动 Mock SSE 服务器
python3 tests/server/mock_sse_server.py 8080 &

# GDScript 集成测试（需要 Godot 和 Mock 服务器运行中）
godot --headless --path demo tests/gdscript/test_runner.tscn

# 完整集成测试（启动服务器 + 运行测试 + 清理）
bash tests/run_integration.sh
```

## 文件结构

```
godot-sse-client/
├── SConstruct                          # SCons 主构建脚本
├── godot-cpp/                          # Git submodule (4.3-stable)
├── src/
│   ├── register_types.h                # GDExtension 入口声明
│   ├── register_types.cpp              # GDExtension 入口实现，注册 SSEClient
│   ├── sse_event.h                     # sse::SSEEvent 结构体（纯 C++）
│   ├── sse_parser.h                    # sse::SSEParser 类声明（纯 C++）
│   ├── sse_parser.cpp                  # sse::SSEParser 类实现
│   ├── sse_client.h                    # SSEClient : Node 声明（Godot 绑定）
│   └── sse_client.cpp                  # SSEClient : Node 实现
├── tests/
│   ├── cpp/
│   │   ├── doctest.h                   # doctest 单头文件
│   │   ├── test_main.cpp               # #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
│   │   ├── test_sse_parser.cpp         # SSEParser 单元测试（22+ 用例）
│   │   └── Makefile                    # 独立编译，不依赖 Godot
│   ├── gdscript/
│   │   ├── test_sse_client.gd          # SSEClient 集成测试
│   │   └── test_runner.tscn            # 测试场景
│   ├── server/
│   │   └── mock_sse_server.py          # Python Mock SSE 服务端
│   └── run_integration.sh              # CI 集成测试脚本
├── demo/
│   ├── project.godot
│   ├── bin/
│   │   └── sse_client.gdextension      # GDExtension 描述文件
│   ├── addons/sse_client/
│   │   ├── plugin.cfg
│   │   └── plugin.gd
│   └── examples/
│       └── ai_agent_chat.gd            # AI Agent 通信示例
└── README.md
```

## 代码规范

### 命名

- 类名: `PascalCase` — `SSEClient`, `SSEParser`
- 方法/函数: `snake_case` — `connect_to_url`, `parse_line`
- 成员变量: `m_` 前缀 — `m_parser`, `m_state`
- 常量/枚举值: `UPPER_SNAKE_CASE` — `STATUS_STREAMING`
- 文件名: `snake_case` — `sse_client.h`, `sse_parser.cpp`

### 命名空间

- 纯 C++ 类（SSEEvent, SSEParser）: `namespace sse { }`
- Godot 绑定类（SSEClient）: 全局命名空间（GDCLASS 宏要求）

### 其他

- 头文件保护: `#pragma once`
- 公开 API 注释: Doxygen 风格 `///`
- Godot 层错误处理: `ERR_FAIL_*` 宏
- 纯 C++ 层错误处理: 返回值
- 字符串转换: 在 SSEClient 边界使用 `String::utf8(ptr, len)` 将 `std::string` 转为 Godot `String`

## 架构设计

### 分层结构

```
┌─────────────────────────────────┐
│  GDScript 层                     │  ai_agent_chat.gd / 用户脚本
├─────────────────────────────────┤
│  SSEClient (godot::Node)        │  Godot 绑定、信号、状态机、HTTPClient 轮询
├─────────────────────────────────┤
│  sse::SSEParser (纯 C++)        │  SSE 协议解析，std::string，可独立测试
│  sse::SSEEvent  (纯 C++)        │  事件数据结构
└─────────────────────────────────┘
```

### 关键约束

1. **`sse_event.h` 和 `sse_parser.h/cpp` 严禁包含任何 godot_cpp 头文件。** 仅使用 C++ 标准库（`<string>`, `<vector>`），确保可脱离 Godot 独立编译和 doctest 单元测试。

2. **SSEClient 使用 `_process()` 轮询模式**，不使用后台线程。HTTPClient::poll() 和 read_response_body_chunk() 均为非阻塞调用，在主线程帧更新中执行。

3. **HTTP 层使用 Godot 内置 HTTPClient**，不引入 libcurl 或其他外部 HTTP 库。HTTPClient 原生支持 TLS（通过 TLSOptions::client()）和分块读取响应体。

4. **SSEParser 遵循 W3C SSE 规范**:
   - data 字段每行追加 `\n`，分发时移除末尾 `\n`
   - data 为空则不分发事件
   - event type 默认为 `"message"`
   - last event id 跨事件持久化
   - 支持 `\n`、`\r\n`、`\r` 三种行终止符
   - 首次 feed 时去除 UTF-8 BOM
   - id 含 NULL 字符时忽略该 id 字段
   - retry 值非纯数字时忽略

### SSEClient 状态机

```
DISCONNECTED
    │ connect_to_url()
    ▼
CONNECTING ──────timeout──────┐
    │ HTTPClient CONNECTED     │
    │ + request() 发送         │
    ▼                          │
READING_HEADERS ──timeout────┤
    │ has_response()           │
    │ 验证 200 + Content-Type  │
    ▼                          │
STREAMING                      │
    │ 服务器关闭 / 连接错误     │
    ▼                          ▼
start_reconnect() ◄────────────┘
    │
    ├── auto_reconnect=false ──► DISCONNECTED
    ├── 超过 max_attempts ─────► DISCONNECTED
    │
    ▼
RECONNECT_WAIT
    │ timer 到期
    └── 重新进入 CONNECTING
```

### SSEClient 公开 API

```
方法:
  connect_to_url(url, headers=[], method="GET", body="") -> Error
  disconnect_from_server() -> void
  is_connected_to_server() -> bool
  get_last_event_id() -> String

属性:
  auto_reconnect: bool = true
  reconnect_time: float = 3.0        # 秒
  max_reconnect_attempts: int = -1   # -1 = 无限
  connect_timeout: float = 30.0      # 秒

信号:
  sse_connected()
  sse_disconnected()
  sse_event_received(event_type: String, data: String, id: String)
  sse_error(error_message: String)
```

### AI Agent 使用模式（POST + SSE 流式响应）

```gdscript
var client = SSEClient.new()
client.auto_reconnect = false
client.sse_event_received.connect(func(type, data, id):
    if data == "[DONE]":
        client.disconnect_from_server()
        return
    var json = JSON.parse_string(data)
    # 处理流式 delta...
)
var headers = PackedStringArray([
    "Content-Type: application/json",
    "Authorization: Bearer YOUR_KEY"
])
var body = JSON.stringify({"model": "gpt-4", "stream": true, "messages": [...]})
client.connect_to_url("https://api.openai.com/v1/chat/completions", headers, "POST", body)
```

## Mock 服务器端点

测试时启动 `python3 tests/server/mock_sse_server.py 8080`，提供以下端点:

| 端点 | 方法 | 行为 |
|---|---|---|
| `/events` | GET | 发送 3 个标准事件后关闭 |
| `/rapid` | GET | 快速发送 100 个事件 |
| `/events-with-id` | GET | 无 Last-Event-ID 发 id:100; 有则发 id:101 |
| `/reconnect-test` | GET | 发 1 个事件后关闭 |
| `/retry-override` | GET | 发 retry:500 后关闭 |
| `/multiline` | GET | 发多行 data 事件 |
| `/error-404` | GET | 返回 404 |
| `/wrong-type` | GET | 返回 200 但 Content-Type: application/json |
| `/hang` | GET | 接受连接但不响应（超时测试） |
| `/v1/chat/completions` | POST | 模拟 OpenAI 流式响应，需 Bearer token |
| `/chat` | POST | 发 5 个 chunk 事件 |

## 开发顺序

```
步骤1: 项目脚手架（SConstruct, register_types, .gdextension, 测试框架）
步骤2: SSEEvent + SSEParser（纯 C++，doctest 单元测试 22+ 用例）
步骤3: SSEClient 节点骨架（构造/析构、绑定、URL 解析、连接/断开、超时）
步骤4: 流数据处理（poll_reading_headers、poll_streaming、信号分发）
步骤5: 自动重连（start_reconnect、poll_reconnect_wait、Last-Event-ID）
步骤6: 跨平台构建验证
步骤7: 编辑器插件 + AI Agent 示例 + 端到端集成测试
```

每步完成后执行该步测试，全部通过后进入下一步。步骤 2 可与步骤 1 并行。

## 常见注意事项

- `disconnect_from_server()` 已做防重复保护，DISCONNECTED 状态下调用直接 return
- 构造函数中 `set_process(false)`，仅 connect 后启用
- 析构函数调用 `cleanup_connection()` 防止资源泄漏
- `poll_connecting()` 中连接成功后立即调用 `request()` 发送 HTTP 请求，无独立 REQUESTING 状态
- 重连成功（进入 STREAMING）时 `m_reconnect_count` 归零
- HTTP 204 响应不触发重连（服务器明确指示停止）
- `String::utf8(ptr, len)` 用于 std::string → godot::String 转换，确保 UTF-8 正确性
