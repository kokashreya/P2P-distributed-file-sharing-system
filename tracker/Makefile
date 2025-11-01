# === Makefile for Client ===

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Iheaders    # include headers folder
LDFLAGS  := -lpthread

# Source files
SRCS := $(wildcard headers/*.cpp) \
        $(wildcard managers/*.cpp) \
        tracker.cpp

OBJS := $(SRCS:.cpp=.o)
TARGET := tracker

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Pattern rule for object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
