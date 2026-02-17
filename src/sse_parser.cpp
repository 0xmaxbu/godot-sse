#include "sse_parser.h"

namespace sse {

SSEParser::SSEParser() : m_first_feed(true) {
}

SSEParser::~SSEParser() {
}

void SSEParser::feed(const std::string& chunk) {
    std::string input = chunk;

    if (m_first_feed) {
        m_first_feed = false;
        if (input.size() >= 3 &&
            static_cast<unsigned char>(input[0]) == 0xEF &&
            static_cast<unsigned char>(input[1]) == 0xBB &&
            static_cast<unsigned char>(input[2]) == 0xBF) {
            input = input.substr(3);
        }
    }

    m_buffer += input;

    size_t pos = 0;
    while (pos < m_buffer.size()) {
        size_t line_end = std::string::npos;
        size_t skip = 0;

        for (size_t i = pos; i < m_buffer.size(); ++i) {
            if (m_buffer[i] == '\r') {
                line_end = i;
                skip = (i + 1 < m_buffer.size() && m_buffer[i + 1] == '\n') ? 2 : 1;
                break;
            } else if (m_buffer[i] == '\n') {
                line_end = i;
                skip = 1;
                break;
            }
        }

        if (line_end == std::string::npos) {
            break;
        }

        std::string line = m_buffer.substr(pos, line_end - pos);
        pos = line_end + skip;
        process_line(line);
    }

    m_buffer = m_buffer.substr(pos);
}

void SSEParser::process_line(const std::string& line) {
    if (line.empty()) {
        dispatch_event();
        return;
    }

    if (line[0] == ':') {
        return;
    }

    std::string field, value;
    auto colon_pos = line.find(':');
    if (colon_pos == std::string::npos) {
        field = line;
        value = "";
    } else {
        field = line.substr(0, colon_pos);
        size_t value_start = colon_pos + 1;
        if (value_start < line.size() && line[value_start] == ' ') {
            value_start++;
        }
        value = line.substr(value_start);
    }

    if (field == "event") {
        m_current_event.type = value;
    } else if (field == "data") {
        m_current_event.data += value;
        m_current_event.data += "\n";
    } else if (field == "id") {
        if (value.find('\0') == std::string::npos) {
            m_last_event_id = value;
        }
    } else if (field == "retry") {
        bool all_digits = !value.empty();
        for (char c : value) {
            if (c < '0' || c > '9') {
                all_digits = false;
                break;
            }
        }
        if (all_digits) {
            m_current_event.retry_ms = std::stoi(value);
        }
    }
}

void SSEParser::dispatch_event() {
    if (!m_current_event.data.empty() && m_current_event.data.back() == '\n') {
        m_current_event.data.pop_back();
    }

    if (m_current_event.data.empty()) {
        m_current_event = SSEEvent{};
        return;
    }

    if (m_current_event.type.empty()) {
        m_current_event.type = "message";
    }
    m_current_event.id = m_last_event_id;

    m_pending_events.push_back(std::move(m_current_event));
    m_current_event = SSEEvent{};
}

std::vector<SSEEvent> SSEParser::take_events() {
    std::vector<SSEEvent> out = std::move(m_pending_events);
    m_pending_events.clear();
    return out;
}

bool SSEParser::has_events() const {
    return !m_pending_events.empty();
}

void SSEParser::reset() {
    m_buffer.clear();
    m_current_event = SSEEvent{};
    m_pending_events.clear();
    m_last_event_id.clear();
    m_first_feed = true;
}

}
