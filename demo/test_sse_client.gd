extends Node

func _ready():
	print("=== Testing SSEClient GDExtension ===")
	
	if not ClassDB.class_exists("SSEClient"):
		print("✗ SSEClient class NOT found")
		get_tree().quit(1)
		return
	
	print("✓ SSEClient class exists")
	
	var client = ClassDB.instantiate("SSEClient")
	print("✓ SSEClient instance created: ", client)
	
	print("\nProperties:")
	print("  auto_reconnect: ", client.auto_reconnect)
	print("  reconnect_time: ", client.reconnect_time)
	print("  max_reconnect_attempts: ", client.max_reconnect_attempts)
	print("  connect_timeout: ", client.connect_timeout)
	
	print("\nMethods:")
	print("  has method 'connect_to_url': ", client.has_method("connect_to_url"))
	print("  has method 'disconnect_from_server': ", client.has_method("disconnect_from_server"))
	print("  has method 'is_connected_to_server': ", client.has_method("is_connected_to_server"))
	print("  has method 'get_last_event_id': ", client.has_method("get_last_event_id"))
	
	print("\nSignals:")
	var signals = client.get_signal_list()
	for sig in signals:
		if sig.name in ["sse_connected", "sse_disconnected", "sse_event_received", "sse_error"]:
			print("  ✓ ", sig.name)
	
	print("\nURL Parsing Tests:")
	var err = client.connect_to_url("not-a-url")
	print("  Invalid URL returns error: ", err == ERR_INVALID_PARAMETER)
	
	err = client.connect_to_url("http://localhost:9999/events")
	print("  Valid URL returns OK: ", err == OK)
	print("  is_connected_to_server: ", client.is_connected_to_server())
	
	client.disconnect_from_server()
	print("  After disconnect: ", !client.is_connected_to_server())
	
	client.queue_free()
	print("\n=== All tests completed ===")
