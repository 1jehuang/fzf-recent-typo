CXX = g++
CXXFLAGS = -O3 -std=c++17 -pthread -Wall -Wextra
LDFLAGS = -lncurses

TARGET = fzf-recent-typo
PREFIX ?= $(HOME)/.local

.PHONY: all clean install

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/

clean:
	rm -f $(TARGET)
