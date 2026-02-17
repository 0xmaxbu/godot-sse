# Godot SSE Client GDExtension

A production-ready Server-Sent Events (SSE) Client implementation for Godot 4.3+. Enables real-time streaming communication with AI agents and SSE-compatible servers.

## Features

- **W3C SSE Spec Compliant** - Full support for data, event, id, and retry fields
- **Automatic Reconnection** - Configurable retry intervals with server retry hints
- **HTTP/HTTPS with TLS** - Secure connections using Godot's HTTPClient
- **POST Requests** - Custom headers and request body support
- **Non-blocking Design** - Uses `_process()` polling, no background threads
- **Cross-platform** - macOS, Linux, and Windows support
- **Signal-based API** - Easy integration with Godot's scene system

## Installation

### Quick Start

1. **Clone and initialize submodules:**
   ```bash
   git clone https://github.com/your-org/godot-sse.git
   cd godot-sse
   git submodule update --init --recursive
   ```

2. **Build the extension:**
   ```bash
   # macOS
   scons platform=macos target=template_debug -j4
   
   # Linux
   scons platform=linux target=template_debug -j4
   
   # Windows
   scons platform=windows target=template_debug -j4
   ```

3. **Use in your Godot project:**
   - Copy `demo/bin/sse_client.gdextension` to your project's `bin/` folder
   - Include the native library files for your target platforms
   - Or simply copy the entire `demo/` folder as a reference project

### Using as a Plugin

The SSE Client includes an editor plugin for easier development:

1. Copy `demo/addons/sse_client/` to your project's `addons/` folder
2. Enable the plugin in **Project → Project Settings → Plugins**
3. The SSEClient node will be available in the Node dialog

## Quick Start Example

```gdscript
extends Node

@onready var sse_client: SSEClient = $SSEClient

func _ready() -> void:
    # Connect signals
    sse_client.sse_connected.connect(_on_connected)
    sse_client.sse_event_received.connect(_on_event_received)
    sse_client.sse_disconnected.connect(_on_disconnected)
    sse_client.sse_error.connect(_on_error)
    
    # Configure for your use case
    sse_client.auto_reconnect = true
    sse_client.reconnect_time = 3.0
    
    # Connect to SSE endpoint
    sse_client.connect_to_url("https://api.example.com/events")

func _on_connected() -> void:
    print("Connected to SSE stream!")

func _on_event_received(event_type: String, data: String, event_id: String) -> void:
    print("Received event: ", event_type)
    print("Data: ", data)
    print("ID: ", event_id)

func _on_disconnected() -> void:
    print("Disconnected from server")

func _on_error(message: String) -> void:
    print("Error: ", message)
```

## API Reference

### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `auto_reconnect` | `bool` | `true` | Enable automatic reconnection after disconnect |
| `reconnect_time` | `float` | `5.0` | Seconds to wait between reconnection attempts |
| `max_reconnect_attempts` | `int` | `-1` | Maximum reconnection attempts (-1 = unlimited) |
| `connect_timeout` | `float` | `10.0` | Connection timeout in seconds |

### Methods

#### `connect_to_url(url: String, headers: PackedStringArray = [], method: String = "GET", body: String = "") -> Error`

Connect to an SSE endpoint.

- `url`: The SSE endpoint URL (http or https)
- `headers`: Optional custom HTTP headers
- `method`: HTTP method ("GET" or "POST")
- `body`: Request body for POST requests

```gdscript
# Basic GET connection
sse_client.connect_to_url("https://api.example.com/stream")

# POST with headers and body
var headers = PackedStringArray([
    "Content-Type: application/json",
    "Authorization: Bearer " + api_key
])
var body = JSON.stringify({"message": "Hello"})
sse_client.connect_to_url("https://api.example.com/chat", headers, "POST", body)
```

#### `disconnect_from_server() -> void`

Disconnect from the SSE endpoint. This also cancels any pending reconnection attempts.

```gdscript
sse_client.disconnect_from_server()
```

#### `is_connected_to_server() -> bool`

Check if currently connected to the server.

```gdscript
if sse_client.is_connected_to_server():
    print("Currently streaming events")
```

### Signals

#### `sse_connected()`

Emitted when successfully connected to the SSE endpoint and headers have been validated.

```gdscript
func _on_connected() -> void:
    print("Streaming ready!")
```

#### `sse_event_received(event_type: String, data: String, event_id: String)`

Emitted when a complete SSE event is received and parsed.

- `event_type`: The event type field (defaults to "message" if empty)
- `data`: The event data (may contain multiple lines)
- `event_id`: The event ID field (empty if not provided)

```gdscript
func _on_event_received(event_type: String, data: String, event_id: String) -> void:
    if data == "[DONE]":
        # OpenAI chat completion marker
        return
    
    var json = JSON.parse_string(data)
    if json and json.has("content"):
        print("Received: ", json["content"])
```

#### `sse_disconnected()`

Emitted when the connection is closed (either by client or server).

```gdscript
func _on_disconnected() -> void:
    print("Connection closed")
```

#### `sse_error(message: String)`

Emitted when an error occurs during connection or streaming.

```gdscript
func _on_error(message: String) -> void:
    push_error("SSE Error: " + message)
```

## AI Agent Integration

### OpenAI Chat Completions

The SSE Client works perfectly with OpenAI's streaming chat completions:

```gdscript
extends Control

@export var api_key: String = ""
@export var api_url: String = "https://api.openai.com/v1/chat/completions"

@onready var sse_client: SSEClient = $SSEClient
var full_response: String = ""

func _ready() -> void:
    sse_client.auto_reconnect = false  # AI requests shouldn't auto-reconnect
    sse_client.sse_event_received.connect(_on_event)

func send_message(user_message: String) -> void:
    if sse_client.is_connected_to_server():
        sse_client.disconnect_from_server()
    
    full_response = ""
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

func _on_event(_type: String, data: String, _id: String) -> void:
    if data == "[DONE]":
        print("Complete response: ", full_response)
        sse_client.disconnect_from_server()
        return
    
    var json = JSON.parse_string(data)
    if json and json.has("choices"):
        var content = json["choices"][0].get("delta", {}).get("content", "")
        full_response += content
        # Update your UI with the streaming content
```

### Handling Server Retry Hints

SSE servers can suggest retry intervals using the `retry` field:

```gdscript
# Server sends: retry: 5000\ndata: hello\n\n
# Client will use 5 seconds between reconnections instead of reconnect_time
```

## Testing

### C++ Unit Tests

The SSE parser has 24 comprehensive unit tests:

```bash
cd tests/cpp
make test
```

Tests cover:
- Basic event parsing
- Multiline data
- Event types
- Event IDs
- UTF-8 BOM handling
- Various line ending formats (\n, \r\n, \r)
- Empty events
- Retry field parsing

### Mock Server

A Python mock SSE server is included for integration testing:

```bash
python3 tests/server/mock_sse_server.py
```

Available endpoints:
- `/events` - Basic 3 events
- `/rapid` - 100 rapid events
- `/events-with-id` - Events with Last-Event-ID
- `/reconnect-test` - Single event then disconnect
- `/retry-override` - Server retry hint
- `/v1/chat/completions` - OpenAI-compatible format

### GDScript Integration Tests

Test scripts are located in `tests/gdscript/`:
- `test_step4_integration.gd` - Basic streaming tests
- `test_step5_reconnection.gd` - Reconnection behavior tests

Run these from the Godot editor or via command line:

```bash
# Headless test execution (Godot 4.3+)
godot --headless --script tests/gdscript/test_sse_client.gd
```

## Architecture

### State Machine

The SSEClient uses a deterministic state machine:

```
DISCONNECTED → CONNECTING → READING_HEADERS → STREAMING
                                     ↓
                            RECONNECT_WAIT ←─┘
```

- **DISCONNECTED**: Initial state, no connection
- **CONNECTING**: Establishing TCP/TLS connection
- **READING_HEADERS**: Waiting for HTTP response headers
- **STREAMING**: Receiving SSE events
- **RECONNECT_WAIT**: Waiting before reconnection attempt

### Threading Model

The implementation uses non-blocking polling via `_process()`:

- No background threads (simplifies debugging and Godot integration)
- All operations complete within a single frame
- Suitable for real-time applications

### Pure C++ Parser

The SSE parser (`sse_parser.h/cpp`) has no Godot dependencies:

```cpp
// Can be tested independently
#include "sse_parser.h"

sse::SSEParser parser;
parser.feed("data: hello\n\n");
auto events = parser.take_events();
```

## Examples

### Simple Event Display

See `demo/simple_test_reconnect.gd` for a basic example with reconnection testing.

### AI Chat Application

See `demo/examples/ai_agent_chat.gd` for a complete streaming chat UI with OpenAI integration.

### Test Scenes

- `demo/simple_test_reconnect.tscn` - Basic reconnection test
- `demo/test_step5.tscn` - Comprehensive Step 5 tests
- `demo/examples/ai_agent_chat.tscn` - AI chat example UI

## Requirements

- **Godot**: 4.3 or later
- **Compiler**: C++17 compatible (GCC 9+, Clang 10+, MSVC 2019+)
- **Build Tool**: SCons 4.0+

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Add tests for your changes
4. Ensure all tests pass
5. Submit a pull request

## License

MIT License - see LICENSE file for details.

## References

- [W3C Server-Sent Events Specification](https://html.spec.whatwg.org/multipage/server-sent-events.html)
- [Godot HTTPClient Documentation](https://docs.godotengine.org/en/4.3/classes/class_httpclient.html)
- [OpenAI Chat Completions API](https://platform.openai.com/docs/api-reference/chat)
