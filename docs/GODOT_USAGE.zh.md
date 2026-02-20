# Godot 使用说明 — SSE Client GDExtension

[English](GODOT_USAGE.en.md) | [中文](GODOT_USAGE.zh.md)

简短说明：本说明展示如何在 Godot 项目与编辑器插件中使用 `SSEClient` 扩展（示例和集成测试均包含于 `demo/` 与 `tests/`）。

---

## 📦 快速上手 (3 步)

1. 构建扩展（或使用 `demo/bin/` 下的预编译库）：

   - macOS / 当前平台（示例）:
     ```bash
     scons platform=macos target=template_debug
     ```

2. 打开 demo 项目：在 Godot 中打开 `demo/`。

3. 启用插件并运行示例场景：
   - 在 Godot 打开 `Project -> Project Settings -> Plugins`，启用 `SSE Client` 插件（`demo/addons/sse_client/plugin.cfg`）。
   - 打开 `demo/examples/ai_agent_chat.tscn` 或运行 `tests/gdscript/test_runner.tscn` 做集成验证。

---

## 🔧 安装与加载

- 将构建产物放入 `demo/bin/`（`.gdextension` 已包含在 `demo/`）。
- `.gdextension` 文件位置：`demo/addons/sse_client/sse_client.gdextension`（兼容性由 `compatibility_minimum` 字段控制）。
- Godot 打开 `demo/` 项目后，会自动加载 `.gdextension` 并注册原生类 `SSEClient`。

> 注意：如果 Godot 控制台出现 `Failed to load GDExtension`，请确认库文件名与 `.gdextension` 中的 `libraries` 字段一致。

---

## 🧩 在场景中使用 `SSEClient`（GDScript）

1. 在场景树中创建节点：`SSEClient`（或通过脚本 `var c = SSEClient.new()`）。
2. 连接信号并发起连接：

```gdscript
@onready var client: SSEClient = $SSEClient

func _ready():
    client.sse_connected.connect(_on_connected)
    client.sse_event_received.connect(_on_event)
    client.sse_disconnected.connect(_on_disconnected)
    client.sse_error.connect(_on_error)

func start_stream():
    var headers = PackedStringArray(["Accept: text/event-stream"])
    client.connect_to_url("http://localhost:8080/events", headers, "GET")

func _on_event(event_type: String, data: String, id: String):
    print("event:", event_type, "data:", data, "id:", id)
```

常用方法与属性（简要）：
- `connect_to_url(url, headers = [], method = "GET", body = "") -> Error`
- `disconnect_from_server()`
- `is_connected_to_server() -> bool`
- `get_last_event_id() -> String`
- 属性：`auto_reconnect`, `reconnect_time`, `max_reconnect_attempts`, `connect_timeout`
- 信号：`sse_connected`, `sse_disconnected`, `sse_event_received(event_type, data, id)`, `sse_error(message)`

---

## 🤖 示例：与流式 AI（OpenAI 风格）交互

- 参考示例：`demo/examples/ai_agent_chat.gd`
- 关键点：请求使用 `POST`、`Content-Type: application/json`、body 中设置 `stream: true`。
- 在 `sse_event_received` 中拼接每个 chunk 的 `choices[].delta.content`；当接收到 `data: [DONE]` 时结束流并 `disconnect_from_server()`。

示例片段（处理 [DONE] 与拼接回复）：

```gdscript
func _on_event(_type, data, _id):
    if data == "[DONE]":
        client.disconnect_from_server()
        return
    var parsed = JSON.parse_string(data)
    if parsed and parsed.has("choices"):
        var chunk = parsed["choices"][0].get("delta", {}).get("content", "")
        _full_text += chunk
```

---

## 🔁 重连与健壮性

- 自动重连：`client.auto_reconnect = true`（默认 true）。
- 重连间隔：`client.reconnect_time`（秒）或由服务器通过 `retry: <ms>` 字段临时覆盖。
- 最大重连次数：`client.max_reconnect_attempts`（-1 表示无限次）。
- 手动断开会取消任何等待中的重连（调用 `disconnect_from_server()`）。

建议：在对话类场景（AI 请求）将 `auto_reconnect = false`，避免中断后自动重试导致语义混乱。

---

## 🧪 本地测试与集成验证

- 启动 Mock SSE 服务器（零依赖）：
  ```bash
  python3 tests/server/mock_sse_server.py 8080 &
  ```
- 运行 Godot headless 集成测试：
  ```bash
  godot --headless --path demo tests/gdscript/test_runner.tscn
  ```
- C++ 单元测试（Parser）：
  ```bash
  cd tests/cpp && make run
  ```
- 全自动集成脚本：
  ```bash
  ./tests/run_integration.sh
  ```

预期：示例场景（如 `ai_agent_chat.tscn`）能收到并拼接流式回复，测试场景输出全部通过。

---

## 📚 API 速查（方法 / 信号 / 属性）

Methods
- `connect_to_url(url, headers = PackedStringArray(), method = "GET", body = "") -> Error`
- `disconnect_from_server()`
- `is_connected_to_server() -> bool`
- `get_last_event_id() -> String`

Signals
- `sse_connected`
- `sse_disconnected`
- `sse_event_received(event_type: String, data: String, id: String)`
- `sse_error(error_message: String)`

Properties
- `auto_reconnect: bool`
- `reconnect_time: float` (seconds)
- `max_reconnect_attempts: int`
- `connect_timeout: float` (seconds)

---

## ⚠️ 常见问题与排查小贴士

- "没有事件"：确认服务器返回 `Content-Type: text/event-stream`（参见 `tests/server/mock_sse_server.py`）。
- "加载失败"：检查 `demo/bin/` 下的库名是否与 `sse_client.gdextension` 中 `libraries` 一致。
- 调试服务端交互：查看 `mock_server.log`（mock 服务器会写入请求头日志）。

---

## 参考示例文件

- 插件主体：`demo/addons/sse_client/plugin.cfg`, `demo/addons/sse_client/plugin.gd`
- 演示脚本：`demo/examples/ai_agent_chat.gd`
- 集成测试（GDScript）：`tests/gdscript/test_runner.tscn`, `tests/gdscript/test_sse_client.gd`
- Mock 服务器：`tests/server/mock_sse_server.py`

---

## 验证清单（快速确认）

1. `scons platform=macos target=template_debug` → 成功并在 `demo/bin/` 产生库文件。
2. Godot 打开 `demo/` → `SSEClient` 可创建，插件可启用。
3. 运行 `ai_agent_chat.tscn` 或 `tests/run_integration.sh` → 能收到流并正确拼接。

---

如果需要，我可以把本说明合并进 `README.md` 或 `demo/` 的使用文档中。