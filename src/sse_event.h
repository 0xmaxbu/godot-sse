#pragma once

#include <string>

namespace sse {

struct SSEEvent {
    std::string type;
    std::string data;
    std::string id;
    int retry_ms = -1;
};

}
