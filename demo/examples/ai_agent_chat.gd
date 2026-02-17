extends Control

## AI Agent Chat Example
##
## Demonstrates how to use SSEClient to communicate with OpenAI-compatible
## streaming chat completion APIs. This example shows:
## - Connecting to SSE endpoints with custom headers (API keys)
## - Handling streaming JSON responses
## - Detecting and processing the [DONE] marker
## - Accumulating streaming text chunks into full responses

## API Configuration
@export var api_url: String = "http://localhost:9999/v1/chat/completions"
@export var api_key: String = "test-key"  # Replace with your actual API key

## UI References
@onready var sse_client: SSEClient = $SSEClient
@onready var input_field: TextEdit = $VBoxContainer/InputPanel/MessageInput
@onready var send_button: Button = $VBoxContainer/InputPanel/SendButton
@onready var output_field: RichTextLabel = $VBoxContainer/OutputPanel/ChatOutput
@onready var status_label: Label = $VBoxContainer/StatusBar/StatusLabel
@onready var clear_button: Button = $VBoxContainer/StatusBar/ClearButton

## Internal State
var _full_response: String = ""
var _is_streaming: bool = false


func _ready() -> void:
	# Connect SSEClient signals
	sse_client.sse_connected.connect(_on_sse_connected)
	sse_client.sse_event_received.connect(_on_sse_event_received)
	sse_client.sse_disconnected.connect(_on_sse_disconnected)
	sse_client.sse_error.connect(_on_sse_error)
	
	# Configure SSEClient for AI agent communication
	sse_client.auto_reconnect = false  # AI requests don't need auto-reconnect
	sse_client.connect_timeout = 30.0  # Longer timeout for AI responses
	
	# Connect UI signals
	send_button.pressed.connect(_on_send_pressed)
	clear_button.pressed.connect(_on_clear_pressed)
	input_field.gui_input.connect(_on_input_gui_input)
	
	# Initial status
	_update_status("Ready. Enter a message and click Send.")
	_update_ui_state()


func _on_send_pressed() -> void:
	var user_message = input_field.text.strip_edges()
	
	if user_message.is_empty():
		_update_status("Error: Message cannot be empty")
		return
	
	if _is_streaming:
		_update_status("Error: Already streaming a response")
		return
	
	send_message(user_message)


func _on_clear_pressed() -> void:
	output_field.clear()
	_update_status("Chat cleared. Ready for new message.")


func _on_input_gui_input(event: InputEvent) -> void:
	# Send on Ctrl+Enter / Cmd+Enter
	if event is InputEventKey:
		if event.pressed and event.keycode == KEY_ENTER:
			if event.ctrl_pressed or event.meta_pressed:
				_on_send_pressed()


## Send a message to the AI agent via SSE streaming
func send_message(user_message: String) -> void:
	# Disconnect any existing connection
	if sse_client.is_connected_to_server():
		sse_client.disconnect_from_server()
	
	# Reset state
	_full_response = ""
	_is_streaming = true
	_update_ui_state()
	
	# Add user message to output
	output_field.append_text("[b][color=cyan]You:[/color][/b] " + user_message + "\n\n")
	output_field.append_text("[b][color=green]AI:[/color][/b] ")
	
	# Build request body (OpenAI Chat Completions format)
	var request_body = {
		"model": "gpt-4",
		"messages": [
			{"role": "user", "content": user_message}
		],
		"stream": true
	}
	
	# Build headers with API key
	var headers = PackedStringArray([
		"Content-Type: application/json",
		"Authorization: Bearer " + api_key
	])
	
	# Connect to SSE endpoint with POST request
	var body_json = JSON.stringify(request_body)
	_update_status("Connecting to AI agent...")
	sse_client.connect_to_url(api_url, headers, "POST", body_json)


## SSEClient Signal Handlers

func _on_sse_connected() -> void:
	_update_status("Connected. Streaming response...")


func _on_sse_event_received(_event_type: String, data: String, _event_id: String) -> void:
	# Check for [DONE] marker (OpenAI SSE convention)
	if data == "[DONE]":
		_update_status("Response complete. Ready for new message.")
		_is_streaming = false
		_update_ui_state()
		output_field.append_text("\n\n")
		sse_client.disconnect_from_server()
		return
	
	# Parse JSON chunk
	var json = JSON.parse_string(data)
	if json == null:
		push_warning("Failed to parse JSON: " + data)
		return
	
	# Extract content from OpenAI streaming format
	# Format: {"choices": [{"delta": {"content": "text"}}]}
	if json.has("choices") and json["choices"].size() > 0:
		var choice = json["choices"][0]
		if choice.has("delta"):
			var delta = choice["delta"]
			if delta.has("content"):
				var content = delta["content"]
				_full_response += content
				output_field.append_text(content)


func _on_sse_disconnected() -> void:
	if _is_streaming:
		_update_status("Disconnected during streaming")
		_is_streaming = false
		_update_ui_state()


func _on_sse_error(error_message: String) -> void:
	_update_status("Error: " + error_message)
	output_field.append_text("\n\n[color=red][Error: " + error_message + "][/color]\n\n")
	_is_streaming = false
	_update_ui_state()


## UI Helpers

func _update_status(message: String) -> void:
	status_label.text = message
	print("[AI Chat] " + message)


func _update_ui_state() -> void:
	send_button.disabled = _is_streaming
	input_field.editable = not _is_streaming
