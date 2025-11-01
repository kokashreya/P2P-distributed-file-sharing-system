# === Makefile for Client ===

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wno-deprecated-declarations -D_FILE_OFFSET_BITS=64 -Iheaders    # Compiler flags
LDFLAGS  := -lpthread -lssl -lcrypto      # Linker flags

# Source files
SRCS := $(wildcard headers/*.cpp) \
        $(wildcard commands/*.cpp) \
        client.cpp

OBJS := $(SRCS:.cpp=.o)
TARGET := client

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
