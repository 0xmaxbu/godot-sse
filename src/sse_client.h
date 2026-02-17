#ifndef SSE_CLIENT_H
#define SSE_CLIENT_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "sse_parser.h"

namespace godot {

class SSEClient : public Node {
    GDCLASS(SSEClient, Node)

public:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        READING_HEADERS,
        STREAMING,
        RECONNECT_WAIT
    };

private:
    // Connection state
    State m_state;
    Ref<HTTPClient> m_http_client;
    sse::SSEParser m_parser;

    // Connection parameters (parsed from URL)
    String m_url;
    String m_host;
    String m_path;
    int m_port;
    bool m_use_tls;

    // Request parameters
    PackedStringArray m_custom_headers;
    String m_method;
    String m_body;

    // SSE state
    String m_last_event_id;

    // Reconnection state
    int m_reconnect_count;
    double m_reconnect_timer;
    double m_timeout_timer;

    // Exported properties
    bool m_auto_reconnect;
    double m_reconnect_time;
    int m_max_reconnect_attempts;
    double m_connect_timeout;

protected:
    static void _bind_methods();

public:
    SSEClient();
    ~SSEClient();

    // Public API methods
    Error connect_to_url(const String& url, 
                        const PackedStringArray& headers = PackedStringArray(),
                        const String& method = "GET",
                        const String& body = "");
    void disconnect_from_server();
    bool is_connected_to_server() const;
    String get_last_event_id() const;

    // Property setters/getters
    void set_auto_reconnect(bool enabled);
    bool get_auto_reconnect() const;

    void set_reconnect_time(double seconds);
    double get_reconnect_time() const;

    void set_max_reconnect_attempts(int attempts);
    int get_max_reconnect_attempts() const;

    void set_connect_timeout(double seconds);
    double get_connect_timeout() const;

    // Godot lifecycle
    void _process(double delta) override;

private:
    // Internal helper methods
    bool parse_url(const String& url);
    void cleanup_connection();
    PackedStringArray build_request_headers();
    void start_reconnect();

    // State polling methods
    void poll_connecting(double delta);
    void poll_reading_headers(double delta);
    void poll_streaming();
    void poll_reconnect_wait(double delta);
};

} // namespace godot

#endif // SSE_CLIENT_H
