CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I.

HEADERS := $(wildcard Object/*.hpp Mechanics/*.hpp Platform/*.hpp)

number_snake: Main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) Main.cpp -o $@ -lncurses

run: number_snake
	./number_snake

clean:
	rm -f number_snake

.PHONY: run clean
