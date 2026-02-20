# Godot Usage — SSE Client GDExtension

[English](GODOT_USAGE.en.md) | [中文](GODOT_USAGE.md)

Short summary: this document shows how to use the native `SSEClient` extension inside Godot projects and via the editor plugin. Example scenes and integration tests live in `demo/` and `tests/`.

---

## 📦 Quick start (3 steps)

1. Build the extension (or use the prebuilt library in `demo/bin/`):

   - macOS / example for current platform:
     ```bash
     scons platform=macos target=template_debug
     ```

2. Open the demo project: open the `demo/` folder in Godot.

3. Enable the plugin and run example scenes:
   - In Godot: Project → Project Settings → Plugins → enable `SSE Client` (see `demo/addons/sse_client/plugin.cfg`).
   - Open `demo/examples/ai_agent_chat.tscn` or run `tests/gdscript/test_runner.tscn` to verify integration.

---

## 🔧 Installation & loading

- Put the built library files into `demo/bin/` (the `.gdextension` is already included in `demo/`).
- `.gdextension` path: `demo/addons/sse_client/sse_client.gdextension`.
- Opening the `demo/` project in Godot will load the extension and register the native `SSEClient` class.

> Tip: if Godot logs `Failed to load GDExtension`, confirm that the library file names match the `libraries` entries in `sse_client.gdextension`.

---

## 🧩 Using `SSEClient` in a scene (GDScript)

1. Add an `SSEClient` node to your scene (or create one by script with `SSEClient.new()`).
2. Connect signals and start a connection:

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

Common API (short):
- `connect_to_url(url, headers = [], method = "GET", body = "") -> Error`
- `disconnect_from_server()`
- `is_connected_to_server() -> bool`
- `get_last_event_id() -> String`
- Properties: `auto_reconnect`, `reconnect_time`, `max_reconnect_attempts`, `connect_timeout`
- Signals: `sse_connected`, `sse_disconnected`, `sse_event_received(event_type, data, id)`, `sse_error(message)`

---

## 🤖 Example — streaming AI responses (OpenAI-like)

- See `demo/examples/ai_agent_chat.gd` for a complete example.
- Important: send `POST` with `Content-Type: application/json` and `stream: true` in the request body.
- In `sse_event_received` append the streaming chunks (`choices[].delta.content`) and stop when you receive `data: [DONE]`.

Example snippet:

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

## 🔁 Reconnect & robustness

- `auto_reconnect` (default `true`) enables automatic reconnects.
- `reconnect_time` sets the delay in seconds; the server can override via `retry: <ms>` SSE field.
- `max_reconnect_attempts` sets an upper bound (-1 = unlimited).
- Calling `disconnect_from_server()` cancels pending reconnects.

Recommendation: set `auto_reconnect = false` for single-shot AI requests to avoid duplicate or resumed streams interfering with semantics.

---

## 🧪 Local testing & verification

- Start the included mock server:
  ```bash
  python3 tests/server/mock_sse_server.py 8080 &
  ```
- Run Godot headless integration tests:
  ```bash
  godot --headless --path demo tests/gdscript/test_runner.tscn
  ```
- Run C++ parser unit tests:
  ```bash
  cd tests/cpp && make run
  ```
- Full integration script:
  ```bash
  ./tests/run_integration.sh
  ```

Expected: example scenes (e.g. `ai_agent_chat.tscn`) receive streaming chunks and reconstruct the full response.

---

## 📚 Quick API reference (methods / signals / properties)

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

## ⚠️ Troubleshooting

- No events: confirm the server sends `Content-Type: text/event-stream` (see `tests/server/mock_sse_server.py`).
- Failed to load GDExtension: verify library filenames in `demo/bin/` match `sse_client.gdextension` `libraries` entries.
- Debug server interactions: inspect `mock_server.log` (the mock server logs incoming request headers).

---

## Reference files

- Plugin: `demo/addons/sse_client/plugin.cfg`, `demo/addons/sse_client/plugin.gd`
- Example: `demo/examples/ai_agent_chat.gd`
- Integration tests: `tests/gdscript/test_runner.tscn`, `tests/gdscript/test_sse_client.gd`
- Mock server: `tests/server/mock_sse_server.py`

---

## Quick verification checklist

1. `scons platform=macos target=template_debug` — library produced in `demo/bin/`.
2. Open `demo/` in Godot — `SSEClient` class available and plugin enabled.
3. Run `ai_agent_chat.tscn` or `./tests/run_integration.sh` — streaming response reconstructs correctly.

---

If you want, I can also add this page into the root `README.md` or add an in-project language switch UI for docs.