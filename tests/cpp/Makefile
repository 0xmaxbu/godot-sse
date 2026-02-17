CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I../../src
SRCS = test_main.cpp test_sse_parser.cpp ../../src/sse_parser.cpp
TARGET = test_runner

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
