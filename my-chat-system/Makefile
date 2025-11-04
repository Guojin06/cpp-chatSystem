CXX = g++
CXXFLAGS = -Wall -g -std=c++11 -Isrc
BUILD_DIR = build
TEST_DIR = test

# 目标文件
OBJS = $(BUILD_DIR)/Channel.o $(BUILD_DIR)/EventLoop.o

all: $(BUILD_DIR) $(OBJS) test_channel

# 创建build目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 编译Channel.o
$(BUILD_DIR)/Channel.o: src/Channel.cpp src/Channel.h
	$(CXX) $(CXXFLAGS) -c src/Channel.cpp -o $(BUILD_DIR)/Channel.o

# 编译EventLoop.o
$(BUILD_DIR)/EventLoop.o: src/EventLoop.cpp src/EventLoop.h src/Channel.h
	$(CXX) $(CXXFLAGS) -c src/EventLoop.cpp -o $(BUILD_DIR)/EventLoop.o

# 测试Channel
test_channel: $(OBJS) $(TEST_DIR)/test_channel.cpp
	$(CXX) $(CXXFLAGS) $(TEST_DIR)/test_channel.cpp $(OBJS) -o $(TEST_DIR)/test_channel

clean:
	rm -rf $(BUILD_DIR) $(TEST_DIR)/test_channel

.PHONY: all clean

