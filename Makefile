CXX = g++
CXXFLAGS = -O3 -std=c++17 -pthread -Wall -Wextra
CXXFLAGS += -I./ftxui/include

# FTXUI libraries (order matters for static linking)
FTXUI_LIBS = ./ftxui/build/libftxui-component.a \
             ./ftxui/build/libftxui-dom.a \
             ./ftxui/build/libftxui-screen.a

TARGET = fzf-recent-typo
PREFIX ?= $(HOME)/.local

.PHONY: all clean install

all: $(TARGET)

$(TARGET): main.cpp $(FTXUI_LIBS)
	$(CXX) $(CXXFLAGS) $< -o $@ $(FTXUI_LIBS)

install: $(TARGET)
	install -d $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/

clean:
	rm -f $(TARGET)
