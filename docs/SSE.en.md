# Godot 4.3+ SSE Client GDExtension — Complete development plan (revised)

[English](SSE.en.md) | [中文](SSE.md)

---

## 1. Tech stack & dependencies

- Target engine: Godot 4.3+ (compatibility_minimum = 4.3)
- Language: C++17 (`-std=c++17`)
- Engine binding: godot-cpp `4.3-stable` (Git submodule)
- Build system: SCons
- HTTP/TLS: Godot's built-in `HTTPClient` (no external C++ HTTP dependence)
- C++ unit tests: doctest (single-header)
- Integration tests: GDScript
- Mock server: Python 3 standard library (`http.server`)

Key decision: use Godot's `HTTPClient` rather than libcurl. `HTTPClient` supports TLS and chunked body reads (`read_response_body_chunk()`), which fit SSE streaming needs and keep the code cross-platform and dependency-free.

---

## 2. Coding conventions

- Class names: PascalCase (e.g. `SSEClient`, `SSEParser`)
- Methods/functions: snake_case (e.g. `connect_to_url`, `parse_line`)
- Member variables: `m_` prefix (e.g. `m_parser`)
- Constants/enums: UPPER_SNAKE_CASE
- Filenames: snake_case (e.g. `sse_client.h`)
- Namespace for pure C++ code: `namespace sse { }` (Godot-bound classes remain global)
- Header guards: `#pragma once`
- Public API comments: Doxygen-style `///`
- Error handling: Godot layer uses `ERR_FAIL_*`; pure C++ returns error codes

---

## 3. Project layout (short)

```
godot-sse-client/
├── SConstruct
├── godot-cpp/ (submodule)
├── src/ (SSEClient, parser, event)
├── tests/ (cpp + gdscript + mock server)
├── demo/ (gdextension + plugin + examples)
└── README.md
```

---

## 4. Core class overview

### sse::SSEEvent (plain C++)

- Fields: `type`, `data`, `id`, `retry_ms`
- Purpose: represent a single SSE event

### sse::SSEParser (plain C++)

- feed(const std::string& chunk)
- take_events() -> std::vector<SSEEvent>
- reset(), has_events()

Parser constraints: use only `std::string` and standard containers so the parser can be built and tested outside Godot.

### SSEClient (Godot Node)

- Uses `HTTPClient` for networking and `_process()` polling
- Exposes `connect_to_url`, `disconnect_from_server`, `is_connected_to_server`, `get_last_event_id`
- Signals: `sse_connected`, `sse_disconnected`, `sse_event_received`, `sse_error`
- Properties: `auto_reconnect`, `reconnect_time`, `max_reconnect_attempts`, `connect_timeout`

---

## 5. Development steps (summary)

1. Project scaffold & build (SCons + godot-cpp submodule).
2. Implement and test `sse::SSEParser` (pure C++) using doctest.
3. Implement `SSEClient` node skeleton and register it with Godot.
4. Read streaming body, feed parser, emit signals for parsed events.
5. Implement automatic reconnection and robustness behaviors.
6. Cross-platform build verification and packaging.
7. Editor plugin + AI-agent example + end-to-end integration tests.

Each step has unit/integration tests and explicit acceptance criteria.

---

## 6. Mock SSE server (for integration tests)

The repository includes a small Python mock server used by integration tests: `tests/server/mock_sse_server.py`. It supports endpoints like `/events`, `/rapid`, `/events-with-id`, `/v1/chat/completions` (OpenAI-style streaming), and endpoints for reconnect/retry tests.

---

## 7. Testing & acceptance

- C++ unit tests for parser (many edge cases: BOM, CRLF, split feeds, retry parsing, id handling).
- GDScript integration tests run under Godot (headless mode supported).
- Mock server used to validate streaming, reconnect, headers, and error cases.

---

If you need the original Chinese version, see `SSE.md`.