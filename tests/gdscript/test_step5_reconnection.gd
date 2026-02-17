extends Node

var client: SSEClient
var test_state: Dictionary = {}
var current_test: String = ""

const SERVER_URL = "http://localhost:9999"

func _ready():
	print("\n=== SSEClient Step 5: Reconnection Tests ===\n")
	client = SSEClient.new()
	add_child(client)
	
	client.sse_connected.connect(_on_connected)
	client.sse_disconnected.connect(_on_disconnected)
	client.sse_event_received.connect(_on_event)
	client.sse_error.connect(_on_error)
	
	await get_tree().create_timer(0.5).timeout
	run_all_tests()

func run_all_tests():
	await test_t5_1_auto_reconnect()
	await test_t5_2_reconnect_interval()
	await test_t5_3_retry_override()
	await test_t5_4_max_attempts()
	await test_t5_5_no_auto_reconnect()
	await test_t5_6_disconnect_during_wait()
	await test_t5_7_last_event_id_roundtrip()
	await test_t5_8_counter_reset()
	await test_t5_9_connect_during_reconnect_wait()
	
	print("\n=== All Step 5 Tests Complete ===")
	get_tree().quit()

func reset_test_state():
	test_state.clear()
	test_state.events = []
	test_state.connected_count = 0
	test_state.disconnected_count = 0
	test_state.error_count = 0
	test_state.last_connected_time = 0.0
	test_state.reconnect_intervals = []

func _on_connected():
	test_state.connected_count += 1
	var now = Time.get_ticks_msec() / 1000.0
	
	if test_state.last_connected_time > 0:
		var interval = now - test_state.last_connected_time
		test_state.reconnect_intervals.append(interval)
		print("  [Connected #%d, interval: %.2fs]" % [test_state.connected_count, interval])
	else:
		print("  [Connected #%d]" % test_state.connected_count)
	
	test_state.last_connected_time = now

func _on_disconnected():
	test_state.disconnected_count += 1
	print("  [Disconnected #%d]" % test_state.disconnected_count)

func _on_event(event_type: String, data: String, id: String):
	var event = {"type": event_type, "data": data, "id": id}
	test_state.events.append(event)
	print("  [Event] type='%s', data='%s', id='%s'" % [event_type, data, id])

func _on_error(error_msg: String):
	test_state.error_count += 1
	print("  [Error] %s" % error_msg)

func test_t5_1_auto_reconnect():
	current_test = "T5.1"
	print("\n--- T5.1: Auto Reconnect ---")
	print("Expected: Server closes after 1 event, client reconnects and receives event again")
	
	reset_test_state()
	client.auto_reconnect = true
	client.reconnect_time = 1.0
	client.max_reconnect_attempts = -1
	
	client.connect_to_url(SERVER_URL + "/reconnect-test")
	
	await get_tree().create_timer(5.0).timeout
	
	client.disconnect_from_server()
	
	var pass = test_state.events.size() >= 2 and test_state.connected_count >= 2
	print("Result: %s (events=%d, connects=%d)" % ["PASS" if pass else "FAIL", test_state.events.size(), test_state.connected_count])

func test_t5_2_reconnect_interval():
	current_test = "T5.2"
	print("\n--- T5.2: Reconnect Interval ---")
	print("Expected: reconnect_time=2.0, interval should be >= 1.8s")
	
	reset_test_state()
	client.auto_reconnect = true
	client.reconnect_time = 2.0
	client.max_reconnect_attempts = -1
	
	client.connect_to_url(SERVER_URL + "/reconnect-test")
	
	await get_tree().create_timer(7.0).timeout
	
	client.disconnect_from_server()
	
	var pass = false
	if test_state.reconnect_intervals.size() > 0:
		var first_interval = test_state.reconnect_intervals[0]
		pass = first_interval >= 1.8
		print("Result: %s (first_interval=%.2fs)" % ["PASS" if pass else "FAIL", first_interval])
	else:
		print("Result: FAIL (no reconnection detected)")

func test_t5_3_retry_override():
	current_test = "T5.3"
	print("\n--- T5.3: Retry Field Override ---")
	print("Expected: Server sends retry:500, next reconnect should be ~0.5s")
	
	reset_test_state()
	client.auto_reconnect = true
	client.reconnect_time = 5.0
	client.max_reconnect_attempts = -1
	
	client.connect_to_url(SERVER_URL + "/retry-override")
	
	await get_tree().create_timer(4.0).timeout
	
	client.disconnect_from_server()
	
	var pass = false
	if test_state.reconnect_intervals.size() > 0:
		var first_interval = test_state.reconnect_intervals[0]
		pass = first_interval < 1.0
		print("Result: %s (first_interval=%.2fs, should be ~0.5s)" % ["PASS" if pass else "FAIL", first_interval])
	else:
		print("Result: FAIL (no reconnection detected)")

func test_t5_4_max_attempts():
	current_test = "T5.4"
	print("\n--- T5.4: Max Reconnect Attempts ---")
	print("Expected: max_reconnect_attempts=2, should stop after 2 reconnects")
	
	reset_test_state()
	client.auto_reconnect = true
	client.reconnect_time = 1.0
	client.max_reconnect_attempts = 2
	
	client.connect_to_url(SERVER_URL + "/reconnect-test")
	
	await get_tree().create_timer(8.0).timeout
	
	var pass = test_state.connected_count == 3 and test_state.error_count >= 1
	print("Result: %s (connects=%d, errors=%d)" % ["PASS" if pass else "FAIL", test_state.connected_count, test_state.error_count])

func test_t5_5_no_auto_reconnect():
	current_test = "T5.5"
	print("\n--- T5.5: Auto Reconnect Disabled ---")
	print("Expected: auto_reconnect=false, should emit sse_disconnected without reconnecting")
	
	reset_test_state()
	client.auto_reconnect = false
	client.reconnect_time = 1.0
	client.max_reconnect_attempts = -1
	
	client.connect_to_url(SERVER_URL + "/reconnect-test")
	
	await get_tree().create_timer(3.0).timeout
	
	var pass = test_state.connected_count == 1 and test_state.disconnected_count >= 1
	print("Result: %s (connects=%d, disconnects=%d)" % ["PASS" if pass else "FAIL", test_state.connected_count, test_state.disconnected_count])

func test_t5_6_disconnect_during_wait():
	current_test = "T5.6"
	print("\n--- T5.6: Disconnect During RECONNECT_WAIT ---")
	print("Expected: Manual disconnect during reconnect wait should stop reconnection")
	
	reset_test_state()
	client.auto_reconnect = true
	client.reconnect_time = 5.0
	client.max_reconnect_attempts = -1
	
	client.connect_to_url(SERVER_URL + "/reconnect-test")
	
	await get_tree().create_timer(2.0).timeout
	
	client.disconnect_from_server()
	
	await get_tree().create_timer(5.0).timeout
	
	var pass = test_state.connected_count == 1 and test_state.disconnected_count >= 1
	print("Result: %s (connects=%d, should be 1)" % ["PASS" if pass else "FAIL", test_state.connected_count])

func test_t5_7_last_event_id_roundtrip():
	current_test = "T5.7"
	print("\n--- T5.7: Last-Event-ID Roundtrip ---")
	print("Expected: First connect gets id:100/data:'first', reconnect with Last-Event-ID:100 gets id:101/data:'resumed'")
	
	reset_test_state()
	client.auto_reconnect = true
	client.reconnect_time = 1.0
	client.max_reconnect_attempts = -1
	
	client.connect_to_url(SERVER_URL + "/events-with-id")
	
	await get_tree().create_timer(5.0).timeout
	
	client.disconnect_from_server()
	
	var pass = false
	if test_state.events.size() >= 2:
		var first_event = test_state.events[0]
		var second_event = test_state.events[1]
		pass = (first_event.id == "100" and first_event.data == "first" and 
		        second_event.id == "101" and second_event.data == "resumed")
		print("Result: %s (first: id=%s/data=%s, second: id=%s/data=%s)" % [
			"PASS" if pass else "FAIL",
			first_event.id, first_event.data,
			second_event.id, second_event.data
		])
	else:
		print("Result: FAIL (not enough events: %d)" % test_state.events.size())

func test_t5_8_counter_reset():
	current_test = "T5.8"
	print("\n--- T5.8: Reconnect Counter Reset ---")
	print("Expected: After successful reconnect, counter resets and can retry again")
	
	reset_test_state()
	client.auto_reconnect = true
	client.reconnect_time = 1.0
	client.max_reconnect_attempts = 2
	
	client.connect_to_url(SERVER_URL + "/reconnect-test")
	
	await get_tree().create_timer(6.0).timeout
	
	var pass = test_state.connected_count >= 3
	print("Result: %s (connects=%d, should be >= 3)" % ["PASS" if pass else "FAIL", test_state.connected_count])

func test_t5_9_connect_during_reconnect_wait():
	current_test = "T5.9"
	print("\n--- T5.9: Connect During RECONNECT_WAIT ---")
	print("Expected: Calling connect_to_url() during RECONNECT_WAIT should return error")
	
	reset_test_state()
	client.auto_reconnect = true
	client.reconnect_time = 10.0
	client.max_reconnect_attempts = -1
	
	client.connect_to_url(SERVER_URL + "/reconnect-test")
	
	await get_tree().create_timer(1.5).timeout
	
	var error_result = client.connect_to_url(SERVER_URL + "/events")
	
	client.disconnect_from_server()
	
	var pass = (error_result == ERR_ALREADY_IN_USE)
	print("Result: %s (error_code=%d, expected=%d)" % ["PASS" if pass else "FAIL", error_result, ERR_ALREADY_IN_USE])
