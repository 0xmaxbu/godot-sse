# Godot SSE Client GDExtension（中文）

[English](README.md) | [中文](README.zh.md)

用于 Godot 4.3+ 的生产级 Server-Sent Events (SSE) 客户端扩展，支持与 AI agent 及 SSE 兼容服务器的实时流式通信。

## 功能

- 完整遵循 W3C SSE 规范（支持 data、event、id、retry）
- 自动重连（可配置，并支持服务器的 retry 提示）
- 支持 HTTP/HTTPS 与 TLS（使用 Godot 内置 HTTPClient）
- 支持 POST 请求与自定义 headers/body
- 非阻塞设计（基于 `_process()` 轮询）
- 跨平台（macOS / Linux / Windows）
- 基于信号的易用 API，便于与 Godot 场景集成

## 快速开始

1. 克隆仓库并初始化子模块：

```bash
git clone https://github.com/your-org/godot-sse.git
cd godot-sse
git submodule update --init --recursive
```

2. 构建扩展（示例 macOS）：

```bash
scons platform=macos target=template_debug -j4
```

3. 在 Godot 中使用：
- 将 `demo/bin/sse_client.gdextension` 和对应平台的本地库拷贝到项目的 `bin/` 下，或直接使用 `demo/` 示例项目。
- 启用插件（`demo/addons/sse_client/`）以便在场景树中使用 `SSEClient` 节点。

有关详细使用说明，请参阅 `docs/GODOT_USAGE.zh.md`。

## 示例（OpenAI 风格流式响应）

详见 `demo/examples/ai_agent_chat.gd`，核心要点：使用 `POST` + `Content-Type: application/json` 且 body 中设置 `stream: true`，在 `sse_event_received` 中拼接 `choices[].delta.content`，遇到 `"[DONE]"` 则结束流并断开连接。

## 测试

- 启动 mock 服务器：
  ```bash
  python3 tests/server/mock_sse_server.py 8080 &
  ```
- 运行 Godot headless 集成测试：
  ```bash
  godot --headless --path demo tests/gdscript/test_runner.tscn
  ```
- 运行 C++ 单元测试（Parser）：
  ```bash
  cd tests/cpp && make run
  ```

## 文档

- English docs: `docs/GODOT_USAGE.en.md`, `docs/SSE.en.md`
- 中文 docs: `docs/GODOT_USAGE.zh.md`, `docs/SSE.zh.md`

---

如果需要，我可以把更多仓库文档也翻译成中英双语并互链。