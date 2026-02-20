# Godot 4.3+ SSE Client GDExtension — 完整开发计划（修订版）

[English](SSE.en.md) | [中文](SSE.md)

---

## 一、技术栈与依赖

| 项目 | 选型 | 说明 |
|---|---|---|
| 目标引擎 | Godot 4.3+ | compatibility_minimum = 4.3 |
| 核心语言 | C++17 | `-std=c++17` |
| 引擎绑定 | godot-cpp `4.3-stable` 分支 | Git submodule |
| 构建系统 | SCons | 与 godot-cpp 保持一致 |
| HTTP/TLS | Godot 内置 `HTTPClient` | 通过 godot-cpp 调用，零外部 C++ 依赖 |
| C++ 单元测试 | doctest v2.4+ | 单头文件，仅编译测试时引入 |
| 集成测试 | GDScript 脚本 | 在 Godot 运行时中执行 |
| Mock 服务器 | Python 3 标准库 (`http.server`) | 零第三方依赖 |

**关键设计决策**：不引入 libcurl 等外部 HTTP 库。Godot 的 `HTTPClient` 原生支持 TLS 与分块读取响应体（`read_response_body_chunk()`），满足 SSE 流式读取需求且天然跨平台。SSEParser 使用纯 `std::string`，不依赖 Godot 类型，确保可脱离引擎独立编译和单元测试。

---

## 二、代码规范

```
类名            → PascalCase           (SSEClient, SSEParser)
方法/函数        → snake_case           (connect_to_url, parse_line)
成员变量         → m_ 前缀              (m_parser, m_state)
常量/枚举值      → UPPER_SNAKE_CASE     (STATE_STREAMING)
文件名           → snake_case           (sse_client.h, sse_parser.cpp)
命名空间         → 纯 C++ 类用 namespace sse {};  Godot 绑定类置于全局（GDCLASS 宏要求）
头文件保护       → #pragma once
注释             → 公开 API 使用 Doxygen 风格 (///)
错误处理         → Godot 层用 ERR_FAIL_* 宏;  纯 C++ 层用返回值
```

---

## 三、项目文件结构

```
godot-sse-client/
├── SConstruct
├── godot-cpp/                          # Git submodule (4.3-stable)
├── src/
│   ├── register_types.h
│   ├── register_types.cpp
│   ├── sse_event.h                     # sse::SSEEvent 结构体 (纯 C++)
│   ├── sse_parser.h                    # sse::SSEParser 声明 (纯 C++)
│   ├── sse_parser.cpp                  # sse::SSEParser 实现
│   ├── sse_client.h                    # SSEClient : Node 声明
│   └── sse_client.cpp                  # SSEClient : Node 实现
├── tests/
│   ├── cpp/
│   │   ├── doctest.h
│   │   ├── test_main.cpp              # #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
│   │   ├── test_sse_parser.cpp
│   │   └── Makefile
│   ├── gdscript/
│   │   ├── test_sse_client.gd
│   │   └── test_runner.tscn
│   └── server/
│       └── mock_sse_server.py
├── demo/
│   ├── project.godot
│   ├── bin/
│   │   └── sse_client.gdextension
│   ├── addons/sse_client/
│   │   ├── plugin.cfg
│   │   └── plugin.gd
│   └── examples/
│       └── ai_agent_chat.gd
└── README.md
```

---

## 四、核心类设计总览

### 4.1 `sse::SSEEvent`

```cpp
#pragma once
#include <string>

namespace sse {
struct SSEEvent {
    std::string type;        // 默认空，分发时若空则填为 "message"
    std::string data;
    std::string id;
    int retry_ms = -1;       // -1 = 未设置
};
} // namespace sse
```

### 4.2 `sse::SSEParser`

```cpp
#pragma once
#include "sse_event.h"
#include <string>
#include <vector>

namespace sse {
class SSEParser {
public:
    void feed(const std::string& chunk);
    std::vector<SSEEvent> take_events();
    bool has_events() const;
    void reset();

private:
    std::string m_buffer;
    SSEEvent m_current_event;
    std::vector<SSEEvent> m_pending_events;
    std::string m_last_event_id;   // 跨事件持久化（规范要求）
    bool m_first_feed = true;

    void process_line(const std::string& line);
    void dispatch_event();
};
} // namespace sse
```

### 4.3 `SSEClient`

```cpp
#pragma once
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/classes/tls_options.hpp>
#include "sse_parser.h"

class SSEClient : public godot::Node {
    GDCLASS(SSEClient, godot::Node)

public:
    SSEClient();
    ~SSEClient();

    godot::Error connect_to_url(
        const godot::String& url,
        const godot::PackedStringArray& headers = {},
        const godot::String& method = "GET",
        const godot::String& body = ""
    );
    void disconnect_from_server();
    bool is_connected_to_server() const;
    godot::String get_last_event_id() const;

    void set_auto_reconnect(bool enabled);
    bool get_auto_reconnect() const;
    void set_reconnect_time(double seconds);
    double get_reconnect_time() const;
    void set_max_reconnect_attempts(int attempts);
    int get_max_reconnect_attempts() const;
    void set_connect_timeout(double seconds);
    double get_connect_timeout() const;

    // 信号: sse_connected / sse_disconnected /
    //       sse_event_received(event_type, data, id) / sse_error(message)

protected:
    static void _bind_methods();
    void _process(double delta) override;

private:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        READING_HEADERS,
        STREAMING,
        RECONNECT_WAIT
    };

    State m_state = State::DISCONNECTED;
    godot::Ref<godot::HTTPClient> m_http_client;
    sse::SSEParser m_parser;

    godot::String m_url, m_host, m_path, m_method, m_body;
    godot::PackedStringArray m_custom_headers;
    int m_port = 80;
    bool m_use_tls = false;

    godot::String m_last_event_id;
    bool m_auto_reconnect = true;
    double m_reconnect_time = 3.0;
    int m_max_reconnect_attempts = -1;
    int m_reconnect_count = 0;
    double m_reconnect_timer = 0.0;
    double m_connect_timeout = 30.0;
    double m_timeout_timer = 0.0;

    bool parse_url(const godot::String& url);
    void poll_connecting(double delta);
    void poll_reading_headers(double delta);
    void poll_streaming();
    void poll_reconnect_wait(double delta);
    void start_reconnect();
    void cleanup_connection();
    godot::PackedStringArray build_request_headers();
};
```

---

## 五、分步开发计划

---

### 步骤 1：项目脚手架与构建系统

#### 1.0 测试先行分析

验证方式：SCons 构建成功生成目标动态库文件；将库放入 demo 项目后 Godot 编辑器能无错误加载 GDExtension；C++ 测试框架能独立编译运行。需覆盖 debug/release 两种构建模式。

#### 1.1 任务描述

初始化仓库，引入 godot-cpp 4.3-stable 子模块。编写 `SConstruct`。创建空壳 `register_types.cpp/h`。创建 `.gdextension` 描述文件和 demo 项目。初始化测试目录并放入 doctest.h。

#### 1.2 关键文件伪代码

**SConstruct**：

```python
#!/usr/bin/env python
import os

env = SConscript("godot-cpp/SConstruct")
env.Append(CPPPATH=["src/"])

sources = Glob("src/*.cpp")

library = env.SharedLibrary(
    target="demo/bin/libsse_client{}{}".format(
        env["suffix"], env["SHLIBSUFFIX"]
    ),
    source=sources,
)
Default(library)
```

**register_types.h**：

```cpp
#pragma once
#include <godot_cpp/core/class_db.hpp>

void initialize_sse_client_module(godot::ModuleInitializationLevel p_level);
void uninitialize_sse_client_module(godot::ModuleInitializationLevel p_level);
```

**register_types.cpp**：

```cpp
#include "register_types.h"
#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

void initialize_sse_client_module(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) return;
    // 步骤3中在此注册 SSEClient
}
void uninitialize_sse_client_module(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) return;
}

extern "C" {
GDExtensionBool GDE_EXPORT sse_client_library_init(
    GDExtensionInterfaceGetProcAddress p_get_proc_address,
    const GDExtensionClassLibraryPtr p_library,
    GDExtensionInitialization *r_initialization
) {
    godot::GDExtensionBinding::InitObject init_obj(
        p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize_sse_client_module);
    init_obj.register_terminator(uninitialize_sse_client_module);
    init_obj.set_minimum_library_initialization_level(
        godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}
```

**sse_client.gdextension**：

```ini
[configuration]
entry_symbol = "sse_client_library_init"
compatibility_minimum = "4.3"
reloadable = true

[libraries]
linux.debug.x86_64 = "res://bin/libsse_client.linux.template_debug.x86_64.so"
linux.release.x86_64 = "res://bin/libsse_client.linux.template_release.x86_64.so"
windows.debug.x86_64 = "res://bin/libsse_client.windows.template_debug.x86_64.dll"
windows.release.x86_64 = "res://bin/libsse_client.windows.template_release.x86_64.dll"
macos.debug = "res://bin/libsse_client.macos.template_debug.framework"
macos.release = "res://bin/libsse_client.macos.template_release.framework"
```

**tests/cpp/Makefile**：

```makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I../../src
SRCS = test_main.cpp test_sse_parser.cpp ../../src/sse_parser.cpp
TARGET = test_runner

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
.PHONY: run clean
```

**tests/cpp/test_main.cpp**：

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```

#### 1.3 验收标准

- `scons platform=<当前平台> target=template_debug` 编译成功，库文件出现在 `demo/bin/`
- `scons target=template_release` 同样成功
- Godot 4.3+ 打开 demo 项目，控制台无 GDExtension 加载错误
- `cd tests/cpp && make run` 输出 "0 test cases"

#### 1.4 测试用例

| 编号 | 操作 | 预期 |
|---|---|---|
| T1.1 | `scons target=template_debug` | 退出码 0，.so/.dll 存在 |
| T1.2 | `scons target=template_release` | 退出码 0，库文件存在 |
| T1.3 | Godot 打开 demo 项目 | 无 "Failed to load GDExtension" |
| T1.4 | `cd tests/cpp && make run` | 编译成功，输出 "0 test cases" |

---

### 步骤 2：SSE 事件结构体与流解析器

#### 2.0 测试先行分析

SSEParser 是核心纯逻辑组件，使用 `std::string` 可完全脱离 Godot 用 doctest 测试。需覆盖的全部边界情况：

**正常路径**：单事件、多事件连续、含全字段事件、多行 data 拼接、默认 event 类型 "message"、event id 跨事件持久化。

**分块输入**：事件被拆成多次 `feed()` 调用；行在两次 feed 之间截断。

**格式边界**：`\n`、`\r\n`、`\r` 三种行终止符；UTF-8 BOM；注释行忽略；无冒号后空格；未知字段忽略。

**特殊情况**：空 data 行不产生事件（规范要求）；连续空行仅触发一次分发判断；retry 非数字忽略；id 含 NULL 字符忽略；reset 清空全部状态；空 feed 不崩溃；UTF-8 中文/Emoji；大数据量。

#### 2.1 任务描述

实现 `sse::SSEEvent`（见 4.1）和 `sse::SSEParser`（有状态流解析器），严格遵循 W3C SSE 规范。关键点：data 字段每行追加 `\n`，分发时移除末尾 `\n`，data 为空则不分发；`m_last_event_id` 跨事件持久化。编写完整 doctest 单元测试。

**约束**：`sse_event.h` 和 `sse_parser.h/cpp` 仅使用 C++ 标准库头文件（`<string>`、`<vector>`），不包含任何 godot_cpp 头文件。

#### 2.2 方法签名

```
sse::SSEParser::feed(chunk: const std::string&) -> void
    输入: 原始字节流片段（任意长度，可在任意位置截断）
    输出: 无（内部状态更新，完成的事件追加到 m_pending_events）

sse::SSEParser::take_events() -> std::vector<SSEEvent>
    输出: 所有待取出事件（移动语义），调用后内部队列清空

sse::SSEParser::has_events() -> bool (const)

sse::SSEParser::reset() -> void
    清空 m_buffer, m_current_event, m_pending_events, m_last_event_id, 重置 m_first_feed
```

#### 2.3 伪代码

```cpp
void SSEParser::feed(const std::string& chunk) {
    std::string input = chunk;

    if (m_first_feed) {
        m_first_feed = false;
        // 去除 UTF-8 BOM
        if (input.size() >= 3 &&
            input[0] == '\xEF' && input[1] == '\xBB' && input[2] == '\xBF') {
            input = input.substr(3);
        }
    }

    m_buffer += input;

    // 逐行提取（支持 \r\n, \r, \n）
    size_t pos = 0;
    while (pos < m_buffer.size()) {
        size_t line_end = std::string::npos;
        size_t skip = 0;

        for (size_t i = pos; i < m_buffer.size(); ++i) {
            if (m_buffer[i] == '\r') {
                line_end = i;
                skip = (i + 1 < m_buffer.size() && m_buffer[i + 1] == '\n') ? 2 : 1;
                break;
            } else if (m_buffer[i] == '\n') {
                line_end = i;
                skip = 1;
                break;
            }
        }

        if (line_end == std::string::npos) break; // 不完整行，等下次 feed

        std::string line = m_buffer.substr(pos, line_end - pos);
        pos = line_end + skip;
        process_line(line);
    }

    m_buffer = m_buffer.substr(pos);
}

void SSEParser::process_line(const std::string& line) {
    if (line.empty()) {
        dispatch_event();
        return;
    }
    if (line[0] == ':') return; // 注释

    std::string field, value;
    auto colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        field = line;
        value = "";
    } else {
        field = line.substr(0, colon_pos);
        size_t value_start = colon_pos + 1;
        if (value_start < line.size() && line[value_start] == ' ') {
            value_start++;
        }
        value = line.substr(value_start);
    }

    if (field == "event") {
        m_current_event.type = value;
    } else if (field == "data") {
        m_current_event.data += value;
        m_current_event.data += "\n"; // 规范：每行 data 后追加 \n
    } else if (field == "id") {
        if (value.find('\0') == std::string::npos) {
            m_last_event_id = value; // 跨事件持久化
        }
    } else if (field == "retry") {
        bool all_digits = !value.empty();
        for (char c : value) { if (c < '0' || c > '9') all_digits = false; }
        if (all_digits) {
            m_current_event.retry_ms = std::stoi(value);
        }
    }
    // 未知 field 忽略
}

void SSEParser::dispatch_event() {
    // 规范：移除 data 末尾 \n
    if (!m_current_event.data.empty() && m_current_event.data.back() == '\n') {
        m_current_event.data.pop_back();
    }

    // 规范：data 为空则不分发
    if (m_current_event.data.empty()) {
        m_current_event = SSEEvent{};
        return;
    }

    // 空 type 默认为 "message"
    if (m_current_event.type.empty()) {
        m_current_event.type = "message";
    }
    m_current_event.id = m_last_event_id; // 取当前持久化的 id

    m_pending_events.push_back(std::move(m_current_event));
    m_current_event = SSEEvent{};
}

std::vector<SSEEvent> SSEParser::take_events() {
    std::vector<SSEEvent> out = std::move(m_pending_events);
    m_pending_events.clear();
    return out;
}

void SSEParser::reset() {
    m_buffer.clear();
    m_current_event = SSEEvent{};
    m_pending_events.clear();
    m_last_event_id.clear();
    m_first_feed = true;
}
```

#### 2.4 验收标准

- `tests/cpp/test_sse_parser.cpp` 包含至少 22 个 TEST_CASE
- `make -C tests/cpp run` 全部通过
- 覆盖 2.0 中列出的全部边界情况

#### 2.5 测试用例

| 编号 | 场景 | feed 输入 | 预期事件 |
|---|---|---|---|
| T2.1 | 最简单事件 | `"data: hello\n\n"` | `[{type:"message", data:"hello"}]` |
| T2.2 | 自定义类型 | `"event: ping\ndata: ok\n\n"` | `[{type:"ping", data:"ok"}]` |
| T2.3 | 多行 data | `"data: L1\ndata: L2\n\n"` | `[{data:"L1\nL2"}]` |
| T2.4 | 含全字段 | `"id: 42\nevent: msg\ndata: hi\nretry: 5000\n\n"` | `[{type:"msg", data:"hi", id:"42", retry:5000}]` |
| T2.5 | 连续两事件 | `"data: a\n\ndata: b\n\n"` | `[{data:"a"}, {data:"b"}]` |
| T2.6 | 注释忽略 | `": comment\ndata: x\n\n"` | `[{data:"x"}]` |
| T2.7 | 分块 feed | feed `"da"` 再 feed `"ta: split\n\n"` | `[{data:"split"}]` |
| T2.8 | `\r\n` 行终止 | `"data: crlf\r\n\r\n"` | `[{data:"crlf"}]` |
| T2.9 | 单 `\r` 行终止 | `"data: cr\r\r"` | `[{data:"cr"}]` |
| T2.10 | UTF-8 BOM | `"\xEF\xBB\xBFdata: bom\n\n"` | `[{data:"bom"}]` |
| T2.11 | 无冒号后空格 | `"data:nospace\n\n"` | `[{data:"nospace"}]` |
| T2.12 | 单个空 data 行 | `"data:\n\n"` | `[]`（规范：data="" 不分发） |
| T2.13 | 两个空 data 行 | `"data:\ndata:\n\n"` | `[{data:"\n"}]` (两个 \n 去末尾一个) |
| T2.14 | 仅注释 | `": comment\n\n"` | `[]` |
| T2.15 | retry 非数字 | `"retry: abc\ndata: x\n\n"` | `[{data:"x", retry:-1}]` |
| T2.16 | 未知字段 | `"foo: bar\ndata: x\n\n"` | `[{data:"x"}]` |
| T2.17 | 无 data 不分发 | `"event: ping\n\n"` | `[]` |
| T2.18 | 连续空行 | `"data: x\n\n\n\n"` | `[{data:"x"}]` 仅一个 |
| T2.19 | reset | feed `"data: par"` → reset → feed `"data: new\n\n"` | `[{data:"new"}]` |
| T2.20 | id 含 NULL | `"id: a\x00b\ndata: x\n\n"` | `[{data:"x", id:""}]` id 不更新 |
| T2.21 | 空 feed | feed `""` | `[]` 无崩溃 |
| T2.22 | id 跨事件持久 | `"id: 42\ndata: a\n\ndata: b\n\n"` | `[{data:"a",id:"42"}, {data:"b",id:"42"}]` |
| T2.23 | UTF-8 中文 | `"data: 你好世界\n\n"` | `[{data:"你好世界"}]` |
| T2.24 | 大数据 64KB | `"data: " + 65536×'A' + "\n\n"` | data.length == 65536 |

---

### 步骤 3：SSEClient 节点 — 连接管理与生命周期

#### 3.0 测试先行分析

测试需验证：URL 解析对 http/https/自定义端口/带路径/带查询参数的正确处理；非法 URL 返回 `ERR_INVALID_PARAMETER`；重复连接返回 `ERR_ALREADY_IN_USE`；disconnect 防重复调用（不发射多余信号）；各状态中调用 disconnect 均安全；节点被 queue_free 时不泄漏资源；Godot 编辑器能正常创建和检视该节点。

#### 3.1 任务描述

实现 SSEClient 类骨架：构造/析构函数、`_bind_methods()`、`parse_url()`、`connect_to_url()`、`disconnect_from_server()`、`is_connected_to_server()`、`get_last_event_id()`、所有属性 getter/setter、`_process()` 调度框架、`poll_connecting()` 连接轮询。在 `register_types.cpp` 中注册 SSEClient。

#### 3.2 方法签名

```
SSEClient::SSEClient()
    set_process(false)，初始化默认值

SSEClient::~SSEClient()
    调用 cleanup_connection()

SSEClient::connect_to_url(url, headers, method, body) -> godot::Error
    OK / ERR_INVALID_PARAMETER / ERR_ALREADY_IN_USE

SSEClient::disconnect_from_server() -> void
    已 DISCONNECTED 时直接 return（防重复）

SSEClient::is_connected_to_server() -> bool
    m_state != State::DISCONNECTED

SSEClient::get_last_event_id() -> godot::String

SSEClient::parse_url(url) -> bool (private)
    填充 m_host, m_port, m_path, m_use_tls，失败返回 false

SSEClient::poll_connecting(delta) -> void (private)
    轮询 HTTPClient 状态，连接成功后发送请求并迁移到 READING_HEADERS

SSEClient::cleanup_connection() -> void (private)
    关闭并释放 HTTPClient，重置 parser
```

#### 3.3 伪代码

```cpp
SSEClient::SSEClient() {
    set_process(false);
}

SSEClient::~SSEClient() {
    cleanup_connection();
}

bool SSEClient::parse_url(const String& url) {
    String u = url.strip_edges();
    if (u.begins_with("https://")) {
        m_use_tls = true; m_port = 443; u = u.substr(8);
    } else if (u.begins_with("http://")) {
        m_use_tls = false; m_port = 80; u = u.substr(7);
    } else {
        return false;
    }

    int path_start = u.find("/");
    String host_port;
    if (path_start == -1) {
        host_port = u; m_path = "/";
    } else {
        host_port = u.substr(0, path_start);
        m_path = u.substr(path_start);
    }

    int colon = host_port.rfind(":");
    if (colon != -1) {
        m_host = host_port.substr(0, colon);
        m_port = host_port.substr(colon + 1).to_int();
    } else {
        m_host = host_port;
    }
    return !m_host.is_empty();
}

Error SSEClient::connect_to_url(const String& url, const PackedStringArray& headers,
                                 const String& method, const String& body) {
    if (m_state != State::DISCONNECTED) return ERR_ALREADY_IN_USE;
    if (!parse_url(url)) return ERR_INVALID_PARAMETER;

    m_url = url;
    m_custom_headers = headers;
    m_method = method;
    m_body = body;
    m_reconnect_count = 0;
    m_timeout_timer = 0.0;
    m_parser.reset();

    m_http_client.instantiate();
    Ref<TLSOptions> tls_opts;
    if (m_use_tls) tls_opts = TLSOptions::client();
    Error err = m_http_client->connect_to_host(m_host, m_port, tls_opts);
    if (err != OK) {
        m_http_client.unref();
        return err;
    }

    m_state = State::CONNECTING;
    set_process(true);
    return OK;
}

void SSEClient::disconnect_from_server() {
    if (m_state == State::DISCONNECTED) return; // 防重复
    cleanup_connection();
    m_state = State::DISCONNECTED;
    set_process(false);
    emit_signal("sse_disconnected");
}

void SSEClient::cleanup_connection() {
    if (m_http_client.is_valid()) {
        m_http_client->close();
        m_http_client.unref();
    }
    m_parser.reset();
}

void SSEClient::_process(double delta) {
    switch (m_state) {
        case State::CONNECTING:      poll_connecting(delta); break;
        case State::READING_HEADERS: poll_reading_headers(delta); break;
        case State::STREAMING:       poll_streaming(); break;
        case State::RECONNECT_WAIT:  poll_reconnect_wait(delta); break;
        default: break;
    }
}

void SSEClient::poll_connecting(double delta) {
    m_timeout_timer += delta;
    if (m_timeout_timer > m_connect_timeout) {
        emit_signal("sse_error", String("Connection timeout"));
        start_reconnect();
        return;
    }

    m_http_client->poll();
    auto status = m_http_client->get_status();

    if (status == HTTPClient::STATUS_CONNECTED) {
        // 连接成功，立即发送 HTTP 请求
        PackedStringArray headers = build_request_headers();
        HTTPClient::Method http_method = (m_method == "POST")
            ? HTTPClient::METHOD_POST : HTTPClient::METHOD_GET;
        Error err = m_http_client->request(http_method, m_path, headers, m_body);
        if (err != OK) {
            emit_signal("sse_error", String("Failed to send request"));
            start_reconnect();
            return;
        }
        m_timeout_timer = 0.0; // 重置超时计时（进入等待响应阶段）
        m_state = State::READING_HEADERS;
    } else if (status == HTTPClient::STATUS_CONNECTING ||
               status == HTTPClient::STATUS_RESOLVING) {
        return; // 等待
    } else {
        emit_signal("sse_error",
            String("Connection failed: status ") + String::num_int64((int)status));
        start_reconnect();
    }
}

String SSEClient::get_last_event_id() const {
    return m_last_event_id;
}
```

**`_bind_methods()`**：

```cpp
void SSEClient::_bind_methods() {
    ClassDB::bind_method(D_METHOD("connect_to_url", "url", "headers", "method", "body"),
        &SSEClient::connect_to_url,
        DEFVAL(PackedStringArray()), DEFVAL("GET"), DEFVAL(""));
    ClassDB::bind_method(D_METHOD("disconnect_from_server"), &SSEClient::disconnect_from_server);
    ClassDB::bind_method(D_METHOD("is_connected_to_server"), &SSEClient::is_connected_to_server);
    ClassDB::bind_method(D_METHOD("get_last_event_id"), &SSEClient::get_last_event_id);

    ClassDB::bind_method(D_METHOD("set_auto_reconnect", "enabled"), &SSEClient::set_auto_reconnect);
    ClassDB::bind_method(D_METHOD("get_auto_reconnect"), &SSEClient::get_auto_reconnect);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_reconnect"), "set_auto_reconnect", "get_auto_reconnect");

    ClassDB::bind_method(D_METHOD("set_reconnect_time", "seconds"), &SSEClient::set_reconnect_time);
    ClassDB::bind_method(D_METHOD("get_reconnect_time"), &SSEClient::get_reconnect_time);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reconnect_time"), "set_reconnect_time", "get_reconnect_time");

    ClassDB::bind_method(D_METHOD("set_max_reconnect_attempts", "attempts"), &SSEClient::set_max_reconnect_attempts);
    ClassDB::bind_method(D_METHOD("get_max_reconnect_attempts"), &SSEClient::get_max_reconnect_attempts);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_reconnect_attempts"), "set_max_reconnect_attempts", "get_max_reconnect_attempts");

    ClassDB::bind_method(D_METHOD("set_connect_timeout", "seconds"), &SSEClient::set_connect_timeout);
    ClassDB::bind_method(D_METHOD("get_connect_timeout"), &SSEClient::get_connect_timeout);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "connect_timeout"), "set_connect_timeout", "get_connect_timeout");

    ADD_SIGNAL(MethodInfo("sse_connected"));
    ADD_SIGNAL(MethodInfo("sse_disconnected"));
    ADD_SIGNAL(MethodInfo("sse_event_received",
        PropertyInfo(Variant::STRING, "event_type"),
        PropertyInfo(Variant::STRING, "data"),
        PropertyInfo(Variant::STRING, "id")));
    ADD_SIGNAL(MethodInfo("sse_error",
        PropertyInfo(Variant::STRING, "error_message")));
}
```

**register_types.cpp 更新**：

```cpp
#include "sse_client.h"

void initialize_sse_client_module(godot::ModuleInitializationLevel p_level) {
    if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) return;
    godot::ClassDB::register_class<SSEClient>();
}
```

#### 3.4 验收标准

- 编译通过，Godot 中 `SSEClient.new()` 可创建实例
- 节点列表中可搜索到 SSEClient，属性面板显示 5 个导出属性
- `ClassDB.class_exists("SSEClient")` 返回 true
- 信号面板显示 4 个信号
- 对非法 URL 返回 ERR_INVALID_PARAMETER

#### 3.5 测试用例

| 编号 | 场景 | 预期 |
|---|---|---|
| T3.1 | URL `http://host:9090/path?q=1` | host="host", port=9090, path="/path?q=1", tls=false |
| T3.2 | URL `https://api.ai.com/v1/chat` | host="api.ai.com", port=443, tls=true |
| T3.3 | URL `http://host` (无路径) | path="/" |
| T3.4 | URL `not-a-url` | 返回 ERR_INVALID_PARAMETER |
| T3.5 | URL `ftp://bad` | 返回 ERR_INVALID_PARAMETER |
| T3.6 | 重复 connect | 第二次返回 ERR_ALREADY_IN_USE |
| T3.7 | disconnect 后状态 | is_connected_to_server() == false |
| T3.8 | 未连接时 disconnect | 无信号发射，无崩溃 |
| T3.9 | 连续两次 disconnect | 仅第一次发射 sse_disconnected |
| T3.10 | CONNECTING 中 disconnect | 状态归位 DISCONNECTED，无崩溃 |
| T3.11 | 连接中 queue_free | 无崩溃无泄漏 |
| T3.12 | 连接 Mock 服务器 | poll_connecting 进入 CONNECTED |
| T3.13 | 连接不存在端口 | sse_error 信号触发 |
| T3.14 | connect_timeout=2, 服务器不响应 | 2秒后 sse_error 含 "timeout" |

---

### 步骤 4：流数据处理与信号分发

#### 4.0 测试先行分析

需测试：GET 请求收到标准 SSE 事件流并通过信号逐个发出；POST 请求带 JSON body 正确发送且收到响应；HTTP 非 200 状态码触发 sse_error；Content-Type 非 `text/event-stream` 触发 sse_error；事件字段通过信号正确传递；大量快速连续事件不丢失；自定义 headers 正确发送。

#### 4.1 任务描述

实现 `poll_reading_headers()`（验证响应头）、`poll_streaming()`（读取流数据并分发事件）、`build_request_headers()`（构建含 Accept/Last-Event-ID 的请求头）。

#### 4.2 方法签名

```
SSEClient::poll_reading_headers(delta) -> void (private)
    轮询 HTTPClient 直到 has_response()，验证 status code 和 Content-Type

SSEClient::poll_streaming() -> void (private)
    读取 body 分块，feed 给 parser，取出事件并 emit_signal

SSEClient::build_request_headers() -> PackedStringArray (private)
    合并: Accept + Cache-Control + Last-Event-ID + 用户自定义 headers
```

#### 4.3 伪代码

```cpp
void SSEClient::poll_reading_headers(double delta) {
    m_timeout_timer += delta;
    if (m_timeout_timer > m_connect_timeout) {
        emit_signal("sse_error", String("Response timeout"));
        start_reconnect();
        return;
    }

    m_http_client->poll();

    if (!m_http_client->has_response()) {
        auto status = m_http_client->get_status();
        if (status == HTTPClient::STATUS_CONNECTION_ERROR ||
            status == HTTPClient::STATUS_DISCONNECTED) {
            emit_signal("sse_error", String("Connection lost before response"));
            start_reconnect();
        }
        return; // STATUS_REQUESTING 等正常中间状态继续等待
    }

    int response_code = m_http_client->get_response_code();
    if (response_code != 200) {
        emit_signal("sse_error",
            String("HTTP ") + String::num_int64(response_code));
        if (response_code == 204) {
            // 204 = 服务器明确指示停止，不重连
            cleanup_connection();
            m_state = State::DISCONNECTED;
            set_process(false);
            emit_signal("sse_disconnected");
        } else {
            start_reconnect();
        }
        return;
    }

    // 验证 Content-Type 含 text/event-stream
    bool valid_ct = false;
    PackedStringArray resp_headers = m_http_client->get_response_headers();
    for (int i = 0; i < resp_headers.size(); i++) {
        String h = resp_headers[i].to_lower();
        if (h.begins_with("content-type:") && h.find("text/event-stream") != -1) {
            valid_ct = true;
            break;
        }
    }
    if (!valid_ct) {
        emit_signal("sse_error", String("Invalid Content-Type"));
        start_reconnect();
        return;
    }

    m_reconnect_count = 0; // 连接成功，重置重连计数
    m_state = State::STREAMING;
    emit_signal("sse_connected");
}

void SSEClient::poll_streaming() {
    m_http_client->poll();
    auto status = m_http_client->get_status();

    if (status == HTTPClient::STATUS_BODY) {
        PackedByteArray chunk = m_http_client->read_response_body_chunk();
        if (chunk.size() > 0) {
            std::string data(reinterpret_cast<const char*>(chunk.ptr()), chunk.size());
            m_parser.feed(data);

            auto events = m_parser.take_events();
            for (const auto& event : events) {
                if (!event.id.empty()) {
                    m_last_event_id = String::utf8(event.id.c_str(), event.id.length());
                }
                if (event.retry_ms >= 0) {
                    m_reconnect_time = event.retry_ms / 1000.0;
                }
                emit_signal("sse_event_received",
                    String::utf8(event.type.c_str(), event.type.length()),
                    String::utf8(event.data.c_str(), event.data.length()),
                    String::utf8(event.id.c_str(), event.id.length()));
            }
        }
    } else if (status == HTTPClient::STATUS_DISCONNECTED ||
               status == HTTPClient::STATUS_CONNECTION_ERROR) {
        emit_signal("sse_error", String("Server closed connection"));
        start_reconnect();
    }
}

PackedStringArray SSEClient::build_request_headers() {
    PackedStringArray headers;
    headers.push_back("Accept: text/event-stream");
    headers.push_back("Cache-Control: no-cache");
    if (!m_last_event_id.is_empty()) {
        headers.push_back("Last-Event-ID: " + m_last_event_id);
    }
    for (int i = 0; i < m_custom_headers.size(); i++) {
        headers.push_back(m_custom_headers[i]);
    }
    return headers;
}
```

#### 4.4 验收标准

- Mock 服务器发送 3 个 GET 事件后关闭 → sse_event_received 恰好触发 3 次且内容正确
- POST + JSON body → Mock 服务器日志确认收到正确 body 和 headers
- HTTP 404 → sse_error 触发含 "404"，不触发 sse_connected
- Content-Type 为 application/json → sse_error 触发
- sse_connected 恰好触发一次

#### 4.5 测试用例

| 编号 | 场景 | 预期 |
|---|---|---|
| T4.1 | GET `/events` (3个事件) | 收到 3 个事件，type="message" |
| T4.2 | POST `/chat` + JSON body | 收到 5 个事件，type="chunk" |
| T4.3 | GET `/error-404` | sse_error 含 "404" |
| T4.4 | GET `/wrong-type` (Content-Type: application/json) | sse_error 含 "Content-Type" |
| T4.5 | 带 id 的事件 | 信号 id 参数正确，get_last_event_id() 更新 |
| T4.6 | 多行 data 事件 | data 参数含 `\n` |
| T4.7 | GET `/rapid` (100个事件) | 恰好收到 100 个事件 |
| T4.8 | 自定义 headers `["Authorization: Bearer tk"]` | Mock 日志确认收到 |
| T4.9 | 回调中调用 disconnect | 后续事件不再发射 |

---

### 步骤 5：自动重连与健壮性

#### 5.0 测试先行分析

需测试：服务器关闭后自动重连成功；重连间隔符合 reconnect_time；retry 字段覆盖间隔；max_reconnect_attempts 上限后停止并发射 sse_disconnected；auto_reconnect=false 不重连；手动 disconnect 取消重连；重连成功后 reconnect_count 归零；Last-Event-ID 在重连请求中正确发送。

#### 5.1 任务描述

实现 `start_reconnect()`、`poll_reconnect_wait()`。确保 `disconnect_from_server()` 能取消 RECONNECT_WAIT 状态。

#### 5.2 方法签名

```
SSEClient::start_reconnect() -> void (private)
    检查 auto_reconnect 和 max_reconnect_attempts → 进入 RECONNECT_WAIT 或 DISCONNECTED

SSEClient::poll_reconnect_wait(delta) -> void (private)
    计时器递减，到期后重新发起连接
```

#### 5.3 伪代码

```cpp
void SSEClient::start_reconnect() {
    cleanup_connection();

    if (!m_auto_reconnect) {
        m_state = State::DISCONNECTED;
        set_process(false);
        emit_signal("sse_disconnected");
        return;
    }

    if (m_max_reconnect_attempts >= 0 && m_reconnect_count >= m_max_reconnect_attempts) {
        m_state = State::DISCONNECTED;
        set_process(false);
        emit_signal("sse_error", String("Max reconnect attempts reached"));
        emit_signal("sse_disconnected");
        return;
    }

    m_reconnect_count++;
    m_reconnect_timer = m_reconnect_time;
    m_state = State::RECONNECT_WAIT;
}

void SSEClient::poll_reconnect_wait(double delta) {
    m_reconnect_timer -= delta;
    if (m_reconnect_timer > 0.0) return;

    m_http_client.instantiate();
    Ref<TLSOptions> tls_opts;
    if (m_use_tls) tls_opts = TLSOptions::client();
    Error err = m_http_client->connect_to_host(m_host, m_port, tls_opts);
    if (err != OK) {
        emit_signal("sse_error", String("Reconnect failed"));
        start_reconnect();
        return;
    }
    m_parser.reset();
    m_timeout_timer = 0.0;
    m_state = State::CONNECTING;
}
```

注意：步骤 4 中 `poll_reading_headers()` 在验证通过、进入 STREAMING 状态前已有 `m_reconnect_count = 0;`，确保成功连接后重连计数归零。

#### 5.4 验收标准

- Mock 发 1 事件后断开 → 自动重连 → 再收到事件
- reconnect_time=2.0 → 两次 sse_connected 间隔 ≥ 1.8s
- retry:500 → 后续重连间隔 ≈ 0.5s
- max_reconnect_attempts=2 → 第 3 次不重连，sse_disconnected 发射
- auto_reconnect=false → 断开后直接 sse_disconnected
- RECONNECT_WAIT 中 disconnect → 立即 DISCONNECTED
- 重连请求 headers 含正确 Last-Event-ID

#### 5.5 测试用例

**Mock 新增端点**：

```
GET /reconnect-test  → 发送 1 个事件后关闭（每次请求都如此）
GET /retry-override  → 发送 "retry: 500\ndata: fast\n\n" 后关闭
GET /events-with-id  → 无 Last-Event-ID 头时发 "id:100\ndata:first"；
                       有 Last-Event-ID:100 时发 "id:101\ndata:resumed"
```

| 编号 | 场景 | 配置 | 预期 |
|---|---|---|---|
| T5.1 | 自动重连 | auto_reconnect=true | 断开后重新收到事件 |
| T5.2 | 重连间隔 | reconnect_time=2.0 | 间隔 ≥ 1.8s |
| T5.3 | retry 覆盖 | 服务器发 retry:500 | 后续间隔 ≈ 0.5s |
| T5.4 | 达到上限 | max_reconnect_attempts=2 | 第 3 次不重连 |
| T5.5 | 禁用重连 | auto_reconnect=false | 直接 sse_disconnected |
| T5.6 | 等待中 disconnect | RECONNECT_WAIT 中调用 | 立即 DISCONNECTED |
| T5.7 | Last-Event-ID 往返 | 首次 id:100, 重连后 | 第二次收到 data:"resumed" |
| T5.8 | 计数归零 | 重连成功后再断 | 可再次完整重试 |
| T5.9 | RECONNECT_WAIT 中 connect | 调用 connect_to_url | 返回 ERR_ALREADY_IN_USE |

---

### 步骤 6：跨平台构建验证与打包

#### 6.0 测试先行分析

验证每个平台的构建产物能正确加载。需确认：输出文件名与 .gdextension 路径完全匹配；Godot 能创建 SSEClient 实例；全部属性和信号可见。

#### 6.1 任务描述

在当前平台执行 debug/release 构建，确认输出文件名与 `.gdextension` 声明匹配。用 Godot 编辑器验证扩展加载和类注册。如有跨平台 CI 环境，对每个目标平台执行构建。

#### 6.2 构建命令

```bash
# Linux
scons platform=linux target=template_debug
scons platform=linux target=template_release
# Windows
scons platform=windows target=template_debug
scons platform=windows target=template_release
# macOS
scons platform=macos target=template_debug
scons platform=macos target=template_release
```

#### 6.3 验收标准

- 当前平台 debug + release 构建均成功
- `demo/bin/` 下存在与 .gdextension 中声明匹配的库文件
- Godot 编辑器打开 demo 项目无错误
- "创建新节点" 可搜索到 SSEClient
- 属性面板显示 auto_reconnect / reconnect_time / max_reconnect_attempts / connect_timeout
- 信号面板显示 sse_connected / sse_disconnected / sse_event_received / sse_error

#### 6.4 测试用例

| 编号 | 操作 | 预期 |
|---|---|---|
| T6.1 | debug 构建 | 库文件存在于指定路径 |
| T6.2 | release 构建 | 库文件存在于指定路径 |
| T6.3 | Godot 编辑器加载 | 控制台无 GDExtension 错误 |
| T6.4 | `ClassDB.class_exists("SSEClient")` | true |
| T6.5 | 创建 SSEClient 节点 | 属性面板 5 个属性、信号面板 4 个信号 |

---

### 步骤 7：Godot 插件与 AI Agent 集成

#### 7.0 测试先行分析

端到端验证：插件启用/禁用正常；模拟 OpenAI 流式 SSE 格式正确接收和拼接；`[DONE]` 标记正确处理；连续请求无状态泄漏；401 错误正确报告。

#### 7.1 任务描述

创建编辑器插件（plugin.cfg + plugin.gd）。编写 AI Agent 通信示例脚本演示与 OpenAI 兼容 API 的 SSE 流式交互。扩展 Mock 服务器添加 AI 模拟端点。编写端到端集成测试运行脚本。

#### 7.2 伪代码

**plugin.cfg**：

```ini
[plugin]
name="SSE Client"
description="SSE Client for AI Agent communication"
author="Your Team"
version="1.0.0"
script="plugin.gd"
```

**plugin.gd**：

```gdscript
@tool
extends EditorPlugin

# SSEClient 已通过 GDExtension 自动注册为原生类
# 此插件提供编辑器层面的辅助（未来可扩展自定义图标、面板等）

func _enter_tree():
    pass

func _exit_tree():
    pass
```

**ai_agent_chat.gd**（核心示例）：

```gdscript
extends Node

@export var api_url: String = "http://localhost:8080/v1/chat/completions"
@export var api_key: String = ""

@onready var sse_client: SSEClient = $SSEClient

var _full_response: String = ""

func _ready():
    sse_client.sse_connected.connect(_on_connected)
    sse_client.sse_event_received.connect(_on_event)
    sse_client.sse_disconnected.connect(_on_disconnected)
    sse_client.sse_error.connect(_on_error)
    sse_client.auto_reconnect = false  # AI 请求不自动重连

func send_message(user_message: String) -> void:
    if sse_client.is_connected_to_server():
        sse_client.disconnect_from_server()
    _full_response = ""
    var body = JSON.stringify({
        "model": "gpt-4",
        "messages": [{"role": "user", "content": user_message}],
        "stream": true
    })
    var headers = PackedStringArray([
        "Content-Type: application/json",
        "Authorization: Bearer " + api_key
    ])
    sse_client.connect_to_url(api_url, headers, "POST", body)

func _on_connected():
    print("Streaming started...")

func _on_event(_type: String, data: String, _id: String):
    if data == "[DONE]":
        print("Complete: ", _full_response)
        sse_client.disconnect_from_server()
        return
    var json = JSON.parse_string(data)
    if json and json.has("choices"):
        var content = json["choices"][0].get("delta", {}).get("content", "")
        _full_response += content

func _on_disconnected():
    print("Disconnected")

func _on_error(msg: String):
    push_error("SSE Error: " + msg)
```

**test_sse_client.gd**（完整集成测试骨架）：

```gdscript
extends Node

const MOCK_BASE = "http://localhost:8080"
var _pass: int = 0
var _fail: int = 0

func _ready():
    print("=== SSE Client Integration Tests ===")
    await _run_all()
    print("=== %d passed, %d failed ===" % [_pass, _fail])
    if OS.has_feature("headless"):
        get_tree().quit(0 if _fail == 0 else 1)

func _run_all():
    await test_get_basic_events()
    await test_post_with_body()
    await test_http_error()
    await test_wrong_content_type()
    await test_auto_reconnect()
    await test_last_event_id_roundtrip()
    await test_ai_agent_simulation()
    await test_rapid_events()
    await test_disconnect_prevents_reconnect()
    await test_timeout()
    # ...

func test_get_basic_events():
    var client = SSEClient.new()
    add_child(client)
    var events = []
    client.sse_event_received.connect(
        func(t, d, id): events.append({"type": t, "data": d}))
    client.auto_reconnect = false
    client.connect_to_url(MOCK_BASE + "/events")
    await get_tree().create_timer(2.0).timeout
    _assert_eq(events.size(), 3, "basic: event count")
    _assert_eq(events[0]["data"], "event1", "basic: first event")
    client.disconnect_from_server()
    client.queue_free()

func test_last_event_id_roundtrip():
    var client = SSEClient.new()
    add_child(client)
    var events = []
    client.sse_event_received.connect(
        func(t, d, id): events.append({"data": d, "id": id}))
    client.auto_reconnect = true
    client.reconnect_time = 0.5
    client.connect_to_url(MOCK_BASE + "/events-with-id")
    await get_tree().create_timer(3.0).timeout
    _assert_eq(events[0]["data"], "first", "id-rt: first data")
    _assert_eq(events[1]["data"], "resumed", "id-rt: resumed after reconnect")
    client.disconnect_from_server()
    client.queue_free()

func test_ai_agent_simulation():
    var client = SSEClient.new()
    add_child(client)
    var full = ""
    var done = false
    client.sse_event_received.connect(func(t, d, id):
        if d == "[DONE]":
            done = true
        else:
            var json = JSON.parse_string(d)
            if json and json.has("choices"):
                full += json["choices"][0].get("delta", {}).get("content", ""))
    client.auto_reconnect = false
    var headers = PackedStringArray([
        "Content-Type: application/json", "Authorization: Bearer test-key"])
    client.connect_to_url(MOCK_BASE + "/v1/chat/completions", headers, "POST",
        JSON.stringify({"model": "gpt-4", "stream": true,
                        "messages": [{"role": "user", "content": "Hi"}]}))
    await get_tree().create_timer(3.0).timeout
    _assert_eq(full, "Hello World", "ai: full response")
    _assert_eq(done, true, "ai: received DONE")
    client.queue_free()

func _assert_eq(actual, expected, label: String):
    if actual == expected:
        _pass += 1
    else:
        _fail += 1
        print("  FAIL [%s]: expected %s, got %s" % [label, expected, actual])
```

#### 7.3 验收标准

- Godot 编辑器启用 SSE Client 插件无错误
- AI 模拟测试：拼接结果 == "Hello World"，收到 [DONE] 后断开
- 连续 send_message 两次无泄漏
- 401 错误正确触发 sse_error
- 全部集成测试通过

#### 7.4 测试用例

| 编号 | 场景 | 预期 |
|---|---|---|
| T7.1 | 启用插件 | 编辑器无错误 |
| T7.2 | AI 流式响应 | full_response == "Hello World" |
| T7.3 | [DONE] 处理 | disconnect 执行，sse_disconnected 触发 |
| T7.4 | Headers 验证 | Mock 日志含 Authorization 和 Content-Type |
| T7.5 | Body 验证 | Mock 日志含正确 JSON |
| T7.6 | 连续请求 | 第二次结果独立正确 |
| T7.7 | 无效 API Key | Mock 返回 401，sse_error 触发 |

---

## 六、附录：Mock SSE 服务器完整实现

```python
#!/usr/bin/env python3
"""tests/server/mock_sse_server.py — 零依赖 SSE 测试服务器"""
from http.server import HTTPServer, BaseHTTPRequestHandler
import json, time, threading, sys

class SSEHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        if self.path == "/events":
            self._sse_start()
            for i in range(3):
                self._sse_send(f"data: event{i+1}\n\n")
                time.sleep(0.05)

        elif self.path == "/rapid":
            self._sse_start()
            for i in range(100):
                self._sse_send(f"data: msg-{i}\n\n")
            self.wfile.flush()

        elif self.path == "/events-with-id":
            self._sse_start()
            last_id = self.headers.get("Last-Event-ID", "")
            if last_id == "100":
                self._sse_send("id: 101\ndata: resumed\n\n")
            else:
                self._sse_send("id: 100\ndata: first\n\n")

        elif self.path == "/reconnect-test":
            self._sse_start()
            self._sse_send("data: before-disconnect\n\n")

        elif self.path == "/retry-override":
            self._sse_start()
            self._sse_send("retry: 500\ndata: fast\n\n")

        elif self.path == "/multiline":
            self._sse_start()
            self._sse_send("data: line1\ndata: line2\n\n")

        elif self.path == "/error-404":
            self.send_response(404)
            self.end_headers()

        elif self.path == "/wrong-type":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"error":"not sse"}')

        elif self.path == "/hang":
            # 接受连接但不响应（超时测试）
            time.sleep(120)

        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        content_len = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_len) if content_len > 0 else b""

        if self.path == "/v1/chat/completions":
            auth = self.headers.get("Authorization", "")
            if not auth.startswith("Bearer "):
                self.send_response(401)
                self.end_headers()
                return

            self._sse_start()
            chunks = [
                {"choices": [{"delta": {"role": "assistant"}}]},
                {"choices": [{"delta": {"content": "Hello"}}]},
                {"choices": [{"delta": {"content": " World"}}]},
            ]
            for c in chunks:
                self._sse_send(f"data: {json.dumps(c)}\n\n")
                time.sleep(0.05)
            self._sse_send("data: [DONE]\n\n")

        elif self.path == "/chat":
            self._sse_start()
            for i in range(5):
                self._sse_send(f"event: chunk\ndata: part-{i}\n\n")

        else:
            self.send_response(404)
            self.end_headers()

    def _sse_start(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

    def _sse_send(self, data: str):
        self.wfile.write(data.encode("utf-8"))
        self.wfile.flush()

    def log_message(self, fmt, *args):
        with open("mock_server.log", "a") as f:
            headers_dict = {k: v for k, v in self.headers.items()}
            f.write(f"{self.command} {self.path} {json.dumps(headers_dict)}\n")

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    server = HTTPServer(("localhost", port), SSEHandler)
    print(f"Mock SSE server on :{port}")
    server.serve_forever()
```

**CI 测试运行脚本**（`tests/run_integration.sh`）：

```bash
#!/bin/bash
set -e
python3 tests/server/mock_sse_server.py 8080 &
SERVER_PID=$!
sleep 1
# Godot headless 运行集成测试场景
godot --headless --path demo tests/gdscript/test_runner.tscn
EXIT_CODE=$?
kill $SERVER_PID 2>/dev/null
exit $EXIT_CODE
```

---

## 七、开发顺序与依赖关系

```
步骤1 (脚手架) → 步骤2 (Parser, 纯C++) → 步骤3 (连接管理) → 步骤4 (流处理) → 步骤5 (重连) → 步骤6 (构建验证) → 步骤7 (插件+集成)
```

每一步完成后执行该步的测试，全部通过后进入下一步。步骤 2 可与步骤 1 并行（纯 C++ 无 Godot 依赖）。步骤 3-5 为串行依赖链。步骤 7 依赖全部前置步骤完成。