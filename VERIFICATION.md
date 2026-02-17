# Verification Checklist

This document provides step-by-step verification procedures for the SSE Client GDExtension.

## Prerequisites

- Godot 4.3+ installed
- Project built for your platform
- Mock server running (optional, for integration testing)

## Step 1: Open Godot Editor

```bash
# Open the demo project
cd demo
/Applications/Godot.app/Contents/MacOS/Godot .  # macOS
/path/to/godot .                                 # Linux/Windows
```

## Step 2: Verify GDExtension Loads

**Expected**: No red error messages in the console at startup.

**Console should show**:
```
Loading extension: res://bin/sse_client.gdextension
[GDExtension] Loaded SSE Client version 1.0.0
```

**If you see errors**:
- Check `bin/sse_client.gdextension` exists
- Verify library file matches your platform
- Check file permissions

## Step 3: Verify SSEClient Node Registration

### Method A: Node Dialog

1. Create a new scene
2. Click "Add Child Node" (+ button)
3. Type "SSEClient" in the search box
4. **Expected**: "SSEClient" appears in the list with a native node icon

### Method B: Script Verification

Create a new script:

```gdscript
extends Node

func _ready() -> void:
    # Check class exists
    if ClassDB.class_exists("SSEClient"):
        print("[PASS] SSEClient class registered")
    else:
        print("[FAIL] SSEClient class not found")
    
    # Check parent class
    var parent = ClassDB.get_parent_class("SSEClient")
    print("Parent class: ", parent)
```

**Expected**: "SSEClient class registered" and "Parent class: Node"

## Step 4: Verify Properties

In the Inspector with SSEClient selected:

| Property | Type | Default | Editable |
|----------|------|---------|----------|
| auto_reconnect | bool | true | Yes |
| reconnect_time | float | 5.0 | Yes |
| max_reconnect_attempts | int | -1 | Yes |
| connect_timeout | float | 10.0 | Yes |

**Verification Script**:
```gdscript
extends Node

@onready var client = $SSEClient

func _ready() -> void:
    # Check default values
    assert(client.auto_reconnect == true, "Default auto_reconnect should be true")
    assert(client.reconnect_time == 5.0, "Default reconnect_time should be 5.0")
    assert(client.max_reconnect_attempts == -1, "Default max_reconnect_attempts should be -1")
    assert(client.connect_timeout == 10.0, "Default connect_timeout should be 10.0")
    print("[PASS] All default properties correct")
```

## Step 5: Verify Signals

With SSEClient selected, go to **Node → Signals**:

| Signal | Arguments | Description |
|--------|-----------|-------------|
| sse_connected | () | Connected successfully |
| sse_event_received | (String type, String data, String id) | Received event |
| sse_disconnected | () | Disconnected |
| sse_error | (String message) | Error occurred |

**Verification Script**:
```gdscript
extends Node

@onready var client = $SSEClient

func _ready() -> void:
    # Verify signal connections work
    client.sse_connected.connect(_on_test_connected)
    client.sse_event_received.connect(_on_test_event)
    client.sse_disconnected.connect(_on_test_disconnected)
    client.sse_error.connect(_on_test_error)
    
    # Check signal count
    var signals = ClassDB.class_get_signal_list("SSEClient")
    print("Signal count: ", signals.size())
    assert(signals.size() == 4, "Should have 4 signals")
    print("[PASS] All 4 signals registered")

func _on_test_connected(): pass
func _on_test_event(t, d, i): pass
func _on_test_disconnected(): pass
func _on_test_error(m): pass
```

## Step 6: Verify Plugin Integration

### Enable Plugin

1. Go to **Project → Project Settings → Plugins**
2. Find "SSE Client" in the list
3. Check "Enable"
4. **Expected**: Plugin enables without errors

### Plugin Features

The plugin provides:
- Editor integration hooks (for future enhancements)
- No visible UI changes (SSEClient is registered via GDExtension)

## Step 7: Integration Test with Mock Server

### Start Mock Server

```bash
python3 tests/server/mock_sse_server.py &
```

### Test Basic Streaming

1. Open `demo/simple_test_reconnect.tscn`
2. Run the scene
3. Click "Connect"
4. **Expected**:
   - Console: "Connected to SSE server"
   - Events appear in the output area
   - Multiple events received over ~1 second

### Test Reconnection

1. Stop the mock server
2. **Expected**:
   - Client enters RECONNECT_WAIT state
   - Console shows reconnection attempts
   - After mock server restart, connection resumes

## Step 8: Run Unit Tests

### C++ Parser Tests

```bash
cd tests/cpp
make test
```

**Expected output**: All 24 tests passing

```
[doctest] All tests passed!
```

### GDScript Tests

```gdscript
# In Godot, run the test script
var tests = preload("res://tests/gdscript/test_step5_reconnection.gd")
tests.run()
```

## Verification Checklist Summary

| Test | Status | Command/Action |
|------|--------|----------------|
| GDExtension loads | ☐ | Open demo project |
| ClassDB registration | ☐ | Node dialog search or script check |
| Properties visible | ☐ | Inspector with SSEClient selected |
| Signals visible | ☐ | Node → Signals panel |
| Plugin enables | ☐ | Project Settings → Plugins |
| Basic streaming | ☐ | demo/simple_test_reconnect.tscn |
| C++ tests pass | ☐ | `make -C tests/cpp test` |
| Mock server works | ☐ | `python3 tests/server/mock_sse_server.py` |

## Troubleshooting

### "SSEClient not found" in Node dialog

- Check GDExtension loaded without errors
- Verify platform library exists in `bin/`
- Restart Godot editor

### Plugin won't enable

- Check `addons/sse_client/plugin.cfg` syntax
- Verify no errors in console

### Properties not editable

- This is expected in runtime; properties are design-time editable

### Tests fail

- Ensure mock server is running
- Check network connectivity to localhost:9999
- Review test output for specific failure reasons

## Known Issues

- Godot 4.6 beta2 may hang in headless mode (use editor for testing)
- LSP errors in IDE are expected (missing godot-cpp context during development)
