extends Node

## GDScript Integration Tests for SSEClient Step 4
## Tests event streaming, error handling, and signal dispatch

var test_count = 0
var passed_count = 0
var failed_count = 0

var client: Node
var received_events = []
var received_errors = []
var connected_count = 0
var disconnected_count = 0

func _ready():
	print("\n=== SSEClient Step 4 Integration Tests ===\n")
	
	if not ClassDB.class_exists("SSEClient"):
		print("ERROR: SSEClient class not found!")
		get_tree().quit(1)
		return
	
	client = ClassDB.instantiate("SSEClient")
	add_child(client)
	
	client.connect("sse_connected", _on_connected)
	client.connect("sse_disconnected", _on_disconnected)
	client.connect("sse_event_received", _on_event)
	client.connect("sse_error", _on_error)
	
	await get_tree().create_timer(0.5).timeout
	
	await test_t4_1_basic_events()
	await test_t4_3_http_404()
	await test_t4_4_wrong_content_type()
	await test_t4_5_events_with_id()
	await test_t4_6_multiline_data()
	
	print("\n=== Test Summary ===")
	print("Total: ", test_count)
	print("Passed: ", passed_count)
	print("Failed: ", failed_count)
	
	get_tree().quit(0 if failed_count == 0 else 1)

func reset_state():
	received_events.clear()
	received_errors.clear()
	connected_count = 0
	disconnected_count = 0
	if client.is_connected_to_server():
		client.disconnect_from_server()
		await get_tree().create_timer(0.2).timeout

func _on_connected():
	connected_count += 1

func _on_disconnected():
	disconnected_count += 1

func _on_event(event_type: String, data: String, id: String):
	received_events.append({"type": event_type, "data": data, "id": id})

func _on_error(message: String):
	received_errors.append(message)

func assert_true(condition: bool, message: String):
	test_count += 1
	if condition:
		passed_count += 1
		print("  ✓ ", message)
	else:
		failed_count += 1
		print("  ✗ ", message)

func assert_equal(actual, expected, message: String):
	test_count += 1
	if actual == expected:
		passed_count += 1
		print("  ✓ ", message, " (", actual, ")")
	else:
		failed_count += 1
		print("  ✗ ", message, " - Expected: ", expected, ", Got: ", actual)

func test_t4_1_basic_events():
	print("\n[T4.1] GET /events - 3 standard events")
	await reset_state()
	
	var err = client.connect_to_url("http://localhost:9999/events")
	assert_equal(err, OK, "connect_to_url returns OK")
	
	await get_tree().create_timer(2.0).timeout
	
	assert_equal(connected_count, 1, "sse_connected triggered once")
	assert_equal(received_events.size(), 3, "Received 3 events")
	
	if received_events.size() >= 3:
		assert_equal(received_events[0].type, "message", "Event 1 type is 'message'")
		assert_equal(received_events[0].data, "Event 1", "Event 1 data correct")
		assert_equal(received_events[1].data, "Event 2", "Event 2 data correct")
		assert_equal(received_events[2].data, "Event 3", "Event 3 data correct")

func test_t4_3_http_404():
	print("\n[T4.3] GET /error-404 - HTTP 404")
	await reset_state()
	
	client.connect_to_url("http://localhost:9999/error-404")
	await get_tree().create_timer(2.0).timeout
	
	assert_equal(connected_count, 0, "sse_connected NOT triggered")
	assert_true(received_errors.size() > 0, "sse_error triggered")
	
	if received_errors.size() > 0:
		assert_true("404" in received_errors[0], "Error message contains '404'")

func test_t4_4_wrong_content_type():
	print("\n[T4.4] GET /wrong-type - Wrong Content-Type")
	await reset_state()
	
	client.connect_to_url("http://localhost:9999/wrong-type")
	await get_tree().create_timer(2.0).timeout
	
	assert_equal(connected_count, 0, "sse_connected NOT triggered")
	assert_true(received_errors.size() > 0, "sse_error triggered")
	
	if received_errors.size() > 0:
		assert_true("Content-Type" in received_errors[0], "Error message mentions Content-Type")

func test_t4_5_events_with_id():
	print("\n[T4.5] GET /with-id - Events with id field")
	await reset_state()
	
	client.connect_to_url("http://localhost:9999/with-id")
	await get_tree().create_timer(2.0).timeout
	
	assert_equal(received_events.size(), 3, "Received 3 events")
	
	if received_events.size() >= 3:
		assert_equal(received_events[0].id, "msg-001", "Event 1 id correct")
		assert_equal(received_events[1].id, "msg-002", "Event 2 id correct")
		assert_equal(received_events[2].id, "msg-003", "Event 3 id correct")
		
		var last_id = client.get_last_event_id()
		assert_equal(last_id, "msg-003", "get_last_event_id() returns last id")

func test_t4_6_multiline_data():
	print("\n[T4.6] GET /multiline - Multi-line data event")
	await reset_state()
	
	client.connect_to_url("http://localhost:9999/multiline")
	await get_tree().create_timer(2.0).timeout
	
	assert_equal(received_events.size(), 1, "Received 1 event")
	
	if received_events.size() >= 1:
		var data = received_events[0].data
		assert_true("\n" in data, "Data contains newline")
		assert_true(data.begins_with("Line 1"), "Data starts with 'Line 1'")
		assert_true("Line 2" in data, "Data contains 'Line 2'")
		assert_true(data.ends_with("Line 3"), "Data ends with 'Line 3'")
