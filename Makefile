# the compiler: gcc for C program, define as g++ for C++
CC = g++

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall -Wextra --std=c++11 -O3 -fipa-pta -Wl,-s
DEFINES = -DNDEBUG

# the build target executable:
HEADERS = memory.hpp nodes.hpp dawg.hpp
TARGET = blumer-blumer

PROFILING = $(TARGET)-profiling $(TARGET).gcda

all: $(TARGET)

.INTERMEDIATE: $(PROFILING)

build-dir:
	mkdir -p build

$(TARGET): build-dir $(HEADERS) $(TARGET).cpp $(PROFILING)
	$(CC) -fprofile-use $(CFLAGS) $(DEFINES) -o build/$(TARGET) $(TARGET).cpp

$(TARGET).gcda: build-dir $(TARGET)-profiling example-data-10
	build/$(TARGET)-profiling build/example-data-10 > /dev/null

$(TARGET)-profiling: build-dir $(HEADERS) $(TARGET).cpp
	$(CC) -fprofile-generate $(CFLAGS) $(DEFINES) -o build/$(TARGET)-profiling $(TARGET).cpp

clean:
	$(RM) build/*

generate-data: build-dir generate-data.cpp
	$(CC) -o build/generate-data generate-data.cpp

example-data-10: build-dir generate-data
	build/generate-data 1337 10000000 > build/example-data-10

example-data-50: build-dir generate-data
	build/generate-data 1337 50000000 > build/example-data-50

clean-example-data:
	$(RM) build/example-data-10
	$(RM) build/example-data-50
