#include "sse_client.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/tls_options.hpp>

using namespace godot;

SSEClient::SSEClient()
    : m_state(State::DISCONNECTED),
      m_port(80),
      m_use_tls(false),
      m_reconnect_count(0),
      m_reconnect_timer(0.0),
      m_timeout_timer(0.0),
      m_auto_reconnect(true),
      m_reconnect_time(3.0),
      m_max_reconnect_attempts(5),
      m_connect_timeout(10.0) {
    set_process(false);
}

SSEClient::~SSEClient() {
    cleanup_connection();
}

void SSEClient::_bind_methods() {
    ClassDB::bind_method(D_METHOD("connect_to_url", "url", "headers", "method", "body"),
        &SSEClient::connect_to_url,
        DEFVAL(PackedStringArray()), DEFVAL("GET"), DEFVAL(""));
    ClassDB::bind_method(D_METHOD("disconnect_from_server"), &SSEClient::disconnect_from_server);
    ClassDB::bind_method(D_METHOD("is_connected_to_server"), &SSEClient::is_connected_to_server);
    ClassDB::bind_method(D_METHOD("get_last_event_id"), &SSEClient::get_last_event_id);

    ClassDB::bind_method(D_METHOD("set_auto_reconnect", "enabled"), &SSEClient::set_auto_reconnect);
    ClassDB::bind_method(D_METHOD("get_auto_reconnect"), &SSEClient::get_auto_reconnect);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_reconnect"), "set_auto_reconnect", "get_auto_reconnect");

    ClassDB::bind_method(D_METHOD("set_reconnect_time", "seconds"), &SSEClient::set_reconnect_time);
    ClassDB::bind_method(D_METHOD("get_reconnect_time"), &SSEClient::get_reconnect_time);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reconnect_time"), "set_reconnect_time", "get_reconnect_time");

    ClassDB::bind_method(D_METHOD("set_max_reconnect_attempts", "attempts"), &SSEClient::set_max_reconnect_attempts);
    ClassDB::bind_method(D_METHOD("get_max_reconnect_attempts"), &SSEClient::get_max_reconnect_attempts);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_reconnect_attempts"), "set_max_reconnect_attempts", "get_max_reconnect_attempts");

    ClassDB::bind_method(D_METHOD("set_connect_timeout", "seconds"), &SSEClient::set_connect_timeout);
    ClassDB::bind_method(D_METHOD("get_connect_timeout"), &SSEClient::get_connect_timeout);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "connect_timeout"), "set_connect_timeout", "get_connect_timeout");

    ADD_SIGNAL(MethodInfo("sse_connected"));
    ADD_SIGNAL(MethodInfo("sse_disconnected"));
    ADD_SIGNAL(MethodInfo("sse_event_received",
        PropertyInfo(Variant::STRING, "event_type"),
        PropertyInfo(Variant::STRING, "data"),
        PropertyInfo(Variant::STRING, "id")));
    ADD_SIGNAL(MethodInfo("sse_error",
        PropertyInfo(Variant::STRING, "error_message")));
}

bool SSEClient::parse_url(const String& url) {
    String u = url.strip_edges();
    
    if (u.begins_with("https://")) {
        m_use_tls = true;
        m_port = 443;
        u = u.substr(8);
    } else if (u.begins_with("http://")) {
        m_use_tls = false;
        m_port = 80;
        u = u.substr(7);
    } else {
        return false;
    }

    int path_start = u.find("/");
    String host_port;
    if (path_start == -1) {
        host_port = u;
        m_path = "/";
    } else {
        host_port = u.substr(0, path_start);
        m_path = u.substr(path_start);
    }

    int colon = host_port.rfind(":");
    if (colon != -1) {
        m_host = host_port.substr(0, colon);
        m_port = host_port.substr(colon + 1).to_int();
    } else {
        m_host = host_port;
    }

    return !m_host.is_empty();
}

Error SSEClient::connect_to_url(const String& url, const PackedStringArray& headers,
                                 const String& method, const String& body) {
    if (m_state != State::DISCONNECTED) {
        return ERR_ALREADY_IN_USE;
    }

    if (!parse_url(url)) {
        return ERR_INVALID_PARAMETER;
    }

    m_url = url;
    m_custom_headers = headers;
    m_method = method;
    m_body = body;
    m_reconnect_count = 0;
    m_timeout_timer = 0.0;
    m_parser.reset();

    m_http_client.instantiate();
    Ref<TLSOptions> tls_opts;
    if (m_use_tls) {
        tls_opts = TLSOptions::client();
    }
    Error err = m_http_client->connect_to_host(m_host, m_port, tls_opts);
    if (err != OK) {
        m_http_client.unref();
        return err;
    }

    m_state = State::CONNECTING;
    set_process(true);
    return OK;
}

void SSEClient::disconnect_from_server() {
    if (m_state == State::DISCONNECTED) {
        return;
    }

    cleanup_connection();
    m_state = State::DISCONNECTED;
    set_process(false);
    emit_signal("sse_disconnected");
}

void SSEClient::cleanup_connection() {
    if (m_http_client.is_valid()) {
        m_http_client->close();
        m_http_client.unref();
    }
    m_parser.reset();
}

bool SSEClient::is_connected_to_server() const {
    return m_state != State::DISCONNECTED;
}

String SSEClient::get_last_event_id() const {
    return m_last_event_id;
}

void SSEClient::set_auto_reconnect(bool enabled) {
    m_auto_reconnect = enabled;
}

bool SSEClient::get_auto_reconnect() const {
    return m_auto_reconnect;
}

void SSEClient::set_reconnect_time(double seconds) {
    m_reconnect_time = seconds;
}

double SSEClient::get_reconnect_time() const {
    return m_reconnect_time;
}

void SSEClient::set_max_reconnect_attempts(int attempts) {
    m_max_reconnect_attempts = attempts;
}

int SSEClient::get_max_reconnect_attempts() const {
    return m_max_reconnect_attempts;
}

void SSEClient::set_connect_timeout(double seconds) {
    m_connect_timeout = seconds;
}

double SSEClient::get_connect_timeout() const {
    return m_connect_timeout;
}

void SSEClient::_process(double delta) {
    switch (m_state) {
        case State::CONNECTING:
            poll_connecting(delta);
            break;
        case State::READING_HEADERS:
            poll_reading_headers(delta);
            break;
        case State::STREAMING:
            poll_streaming();
            break;
        case State::RECONNECT_WAIT:
            poll_reconnect_wait(delta);
            break;
        default:
            break;
    }
}

PackedStringArray SSEClient::build_request_headers() {
    PackedStringArray headers;
    headers.append("Accept: text/event-stream");
    headers.append("Cache-Control: no-cache");
    
    if (!m_last_event_id.is_empty()) {
        headers.append("Last-Event-ID: " + m_last_event_id);
    }
    
    for (int i = 0; i < m_custom_headers.size(); i++) {
        headers.append(m_custom_headers[i]);
    }
    
    return headers;
}

void SSEClient::start_reconnect() {
    cleanup_connection();

    if (!m_auto_reconnect) {
        m_state = State::DISCONNECTED;
        set_process(false);
        emit_signal("sse_disconnected");
        return;
    }

    if (m_max_reconnect_attempts >= 0 && m_reconnect_count >= m_max_reconnect_attempts) {
        m_state = State::DISCONNECTED;
        set_process(false);
        emit_signal("sse_error", String("Max reconnect attempts reached"));
        emit_signal("sse_disconnected");
        return;
    }

    m_reconnect_count++;
    m_reconnect_timer = 0.0;
    m_state = State::RECONNECT_WAIT;
}

void SSEClient::poll_connecting(double delta) {
    m_timeout_timer += delta;
    if (m_timeout_timer > m_connect_timeout) {
        emit_signal("sse_error", String("Connection timeout"));
        start_reconnect();
        return;
    }

    m_http_client->poll();
    auto status = m_http_client->get_status();

    if (status == HTTPClient::STATUS_CONNECTED) {
        PackedStringArray headers = build_request_headers();
        HTTPClient::Method http_method = (m_method == "POST")
            ? HTTPClient::METHOD_POST : HTTPClient::METHOD_GET;
        Error err = m_http_client->request(http_method, m_path, headers, m_body);
        if (err != OK) {
            emit_signal("sse_error", String("Failed to send request"));
            start_reconnect();
            return;
        }
        m_timeout_timer = 0.0;
        m_state = State::READING_HEADERS;
    } else if (status == HTTPClient::STATUS_CONNECTING ||
               status == HTTPClient::STATUS_RESOLVING) {
        return;
    } else {
        emit_signal("sse_error",
            String("Connection failed: status ") + String::num_int64((int)status));
        start_reconnect();
    }
}

void SSEClient::poll_reading_headers(double delta) {
    m_timeout_timer += delta;
    if (m_timeout_timer > m_connect_timeout) {
        emit_signal("sse_error", String("Response timeout"));
        start_reconnect();
        return;
    }

    m_http_client->poll();

    if (!m_http_client->has_response()) {
        auto status = m_http_client->get_status();
        if (status == HTTPClient::STATUS_CONNECTION_ERROR ||
            status == HTTPClient::STATUS_DISCONNECTED) {
            emit_signal("sse_error", String("Connection lost before response"));
            start_reconnect();
        }
        return;
    }

    int response_code = m_http_client->get_response_code();
    if (response_code != 200) {
        emit_signal("sse_error",
            String("HTTP ") + String::num_int64(response_code));
        if (response_code == 204) {
            cleanup_connection();
            m_state = State::DISCONNECTED;
            set_process(false);
            emit_signal("sse_disconnected");
        } else {
            start_reconnect();
        }
        return;
    }

    bool valid_ct = false;
    PackedStringArray resp_headers = m_http_client->get_response_headers();
    for (int i = 0; i < resp_headers.size(); i++) {
        String h = resp_headers[i].to_lower();
        if (h.begins_with("content-type:") && h.find("text/event-stream") != -1) {
            valid_ct = true;
            break;
        }
    }
    if (!valid_ct) {
        emit_signal("sse_error", String("Invalid Content-Type"));
        start_reconnect();
        return;
    }

    m_reconnect_count = 0;
    m_state = State::STREAMING;
    emit_signal("sse_connected");
}

void SSEClient::poll_streaming() {
    m_http_client->poll();
    auto status = m_http_client->get_status();

    if (status == HTTPClient::STATUS_BODY) {
        PackedByteArray chunk = m_http_client->read_response_body_chunk();
        if (chunk.size() > 0) {
            std::string data(reinterpret_cast<const char*>(chunk.ptr()), chunk.size());
            m_parser.feed(data);

            auto events = m_parser.take_events();
            for (const auto& event : events) {
                if (!event.id.empty()) {
                    m_last_event_id = String::utf8(event.id.c_str(), event.id.length());
                }
                if (event.retry_ms >= 0) {
                    m_reconnect_time = event.retry_ms / 1000.0;
                }
                emit_signal("sse_event_received",
                    String::utf8(event.type.c_str(), event.type.length()),
                    String::utf8(event.data.c_str(), event.data.length()),
                    String::utf8(event.id.c_str(), event.id.length()));
            }
        }
    } else if (status == HTTPClient::STATUS_DISCONNECTED ||
               status == HTTPClient::STATUS_CONNECTION_ERROR) {
        emit_signal("sse_error", String("Server closed connection"));
        start_reconnect();
    }
}

void SSEClient::poll_reconnect_wait(double delta) {
    m_reconnect_timer += delta;
    if (m_reconnect_timer < m_reconnect_time) {
        return;
    }

    m_http_client.instantiate();
    Ref<TLSOptions> tls_opts;
    if (m_use_tls) {
        tls_opts = TLSOptions::client();
    }
    Error err = m_http_client->connect_to_host(m_host, m_port, tls_opts);
    if (err != OK) {
        emit_signal("sse_error", String("Reconnect failed"));
        start_reconnect();
        return;
    }
    m_parser.reset();
    m_timeout_timer = 0.0;
    m_state = State::CONNECTING;
}
