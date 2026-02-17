extends Node

var client: SSEClient
var events_received = 0
var connections = 0

func _ready():
	print("\n=== Simple Step 5 Reconnection Test ===\n")
	
	client = SSEClient.new()
	add_child(client)
	
	client.auto_reconnect = true
	client.reconnect_time = 1.0
	client.max_reconnect_attempts = -1
	
	client.sse_connected.connect(_on_connected)
	client.sse_event_received.connect(_on_event)
	client.sse_disconnected.connect(_on_disconnected)
	client.sse_error.connect(_on_error)
	
	print("Connecting to http://localhost:9999/reconnect-test")
	print("Server will send 1 event then close, client should auto-reconnect")
	print("")
	
	client.connect_to_url("http://localhost:9999/reconnect-test")
	
	await get_tree().create_timer(6.0).timeout
	
	client.disconnect_from_server()
	
	print("\n=== Test Results ===")
	print("Events received: %d (expected: >= 2)" % events_received)
	print("Connections: %d (expected: >= 2)" % connections)
	
	if events_received >= 2 and connections >= 2:
		print("Result: PASS")
	else:
		print("Result: FAIL")
	
	get_tree().quit()

func _on_connected():
	connections += 1
	print("[%d] Connected" % connections)

func _on_event(event_type: String, data: String, id: String):
	events_received += 1
	print("[%d] Event: %s" % [events_received, data])

func _on_disconnected():
	print("Disconnected")

func _on_error(msg: String):
	print("Error: %s" % msg)
