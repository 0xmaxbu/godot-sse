#include "doctest.h"
#include "sse_parser.h"

using namespace sse;

TEST_CASE("T2.1: 最简单事件") {
    SSEParser parser;
    parser.feed("data: hello\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == "message");
    CHECK(events[0].data == "hello");
    CHECK(events[0].id == "");
    CHECK(events[0].retry_ms == -1);
}

TEST_CASE("T2.2: 自定义类型") {
    SSEParser parser;
    parser.feed("event: ping\ndata: ok\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == "ping");
    CHECK(events[0].data == "ok");
}

TEST_CASE("T2.3: 多行data") {
    SSEParser parser;
    parser.feed("data: L1\ndata: L2\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "L1\nL2");
}

TEST_CASE("T2.4: 含全字段") {
    SSEParser parser;
    parser.feed("id: 42\nevent: msg\ndata: hi\nretry: 5000\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == "msg");
    CHECK(events[0].data == "hi");
    CHECK(events[0].id == "42");
    CHECK(events[0].retry_ms == 5000);
}

TEST_CASE("T2.5: 连续两事件") {
    SSEParser parser;
    parser.feed("data: a\n\ndata: b\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].data == "a");
    CHECK(events[1].data == "b");
}

TEST_CASE("T2.6: 注释忽略") {
    SSEParser parser;
    parser.feed(": comment\ndata: x\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "x");
}

TEST_CASE("T2.7: 分块feed") {
    SSEParser parser;
    parser.feed("da");
    CHECK(parser.has_events() == false);
    parser.feed("ta: split\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "split");
}

TEST_CASE("T2.8: \\r\\n行终止") {
    SSEParser parser;
    parser.feed("data: crlf\r\n\r\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "crlf");
}

TEST_CASE("T2.9: 单\\r行终止") {
    SSEParser parser;
    parser.feed("data: cr\r\r");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "cr");
}

TEST_CASE("T2.10: UTF-8 BOM") {
    SSEParser parser;
    std::string bom_input;
    bom_input += (char)0xEF;
    bom_input += (char)0xBB;
    bom_input += (char)0xBF;
    bom_input += "data: bom\n\n";
    parser.feed(bom_input);
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "bom");
}

TEST_CASE("T2.11: 无冒号后空格") {
    SSEParser parser;
    parser.feed("data:nospace\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "nospace");
}

TEST_CASE("T2.12: 单个空data行") {
    SSEParser parser;
    parser.feed("data:\n\n");
    auto events = parser.take_events();
    CHECK(events.size() == 0);
}

TEST_CASE("T2.13: 两个空data行") {
    SSEParser parser;
    parser.feed("data:\ndata:\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "\n");
}

TEST_CASE("T2.14: 仅注释") {
    SSEParser parser;
    parser.feed(": comment\n\n");
    auto events = parser.take_events();
    CHECK(events.size() == 0);
}

TEST_CASE("T2.15: retry非数字") {
    SSEParser parser;
    parser.feed("retry: abc\ndata: x\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "x");
    CHECK(events[0].retry_ms == -1);
}

TEST_CASE("T2.16: 未知字段") {
    SSEParser parser;
    parser.feed("foo: bar\ndata: x\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "x");
}

TEST_CASE("T2.17: 无data不分发") {
    SSEParser parser;
    parser.feed("event: ping\n\n");
    auto events = parser.take_events();
    CHECK(events.size() == 0);
}

TEST_CASE("T2.18: 连续空行") {
    SSEParser parser;
    parser.feed("data: x\n\n\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "x");
}

TEST_CASE("T2.19: reset") {
    SSEParser parser;
    parser.feed("data: par");
    parser.reset();
    parser.feed("data: new\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "new");
}

TEST_CASE("T2.20: id含NULL") {
    SSEParser parser;
    std::string input = "id: a";
    input += '\0';
    input += "b\ndata: x\n\n";
    parser.feed(input);
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "x");
    CHECK(events[0].id == "");
}

TEST_CASE("T2.21: 空feed") {
    SSEParser parser;
    parser.feed("");
    auto events = parser.take_events();
    CHECK(events.size() == 0);
}

TEST_CASE("T2.22: id跨事件持久") {
    SSEParser parser;
    parser.feed("id: 42\ndata: a\n\ndata: b\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].data == "a");
    CHECK(events[0].id == "42");
    CHECK(events[1].data == "b");
    CHECK(events[1].id == "42");
}

TEST_CASE("T2.23: UTF-8中文") {
    SSEParser parser;
    parser.feed("data: 你好世界\n\n");
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data == "你好世界");
}

TEST_CASE("T2.24: 大数据64KB") {
    SSEParser parser;
    std::string large_data = "data: ";
    large_data.append(65536, 'A');
    large_data += "\n\n";
    parser.feed(large_data);
    auto events = parser.take_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].data.length() == 65536);
}

