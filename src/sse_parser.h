#pragma once

#include "sse_event.h"
#include <string>
#include <vector>

namespace sse {

class SSEParser {
public:
    SSEParser();
    ~SSEParser();

    void feed(const std::string& chunk);
    std::vector<SSEEvent> take_events();
    bool has_events() const;
    void reset();

private:
    std::string m_buffer;
    SSEEvent m_current_event;
    std::vector<SSEEvent> m_pending_events;
    std::string m_last_event_id;
    bool m_first_feed;

    void process_line(const std::string& line);
    void dispatch_event();
};

}
