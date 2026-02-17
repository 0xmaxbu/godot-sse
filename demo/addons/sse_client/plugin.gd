@tool
extends EditorPlugin

## SSE Client Editor Plugin
##
## This plugin provides the SSEClient node for Godot 4.3+.
## The SSEClient class is registered via GDExtension and is automatically
## available in the editor without additional registration.
##
## This plugin script provides editor-level integration and can be extended
## in the future to add custom icons, inspector panels, or other editor features.

func _enter_tree() -> void:
	# SSEClient is already registered as a native class via GDExtension
	# Future enhancements could add:
	# - Custom editor icon for SSEClient nodes
	# - Inspector plugin for advanced property editing
	# - Dock panel for testing SSE connections
	pass


func _exit_tree() -> void:
	# Cleanup if needed
	pass
