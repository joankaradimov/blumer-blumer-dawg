# the compiler: gcc for C program, define as g++ for C++
CC = g++

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall --std=c++11 -O3 -fipa-pta -Wl,-s
DEFINES = -DNDEBUG

# the build target executable:
TARGET = blumer-blumer

PROFILING = $(TARGET)-profiling $(TARGET).gcda

all: $(TARGET)

.INTERMEDIATE: $(PROFILING)

$(TARGET): $(TARGET).cpp $(PROFILING)
	$(CC) -fprofile-use $(CFLAGS) $(DEFINES) -o $(TARGET) $(TARGET).cpp

$(TARGET).gcda: $(TARGET)-profiling example-data-10
	./$(TARGET)-profiling example-data-10 > /dev/null

$(TARGET)-profiling: $(TARGET).cpp
	$(CC) -fprofile-generate $(CFLAGS) $(DEFINES) -o $(TARGET)-profiling $(TARGET).cpp

clean:
	$(RM) $(TARGET)
	$(RM) generate-data

generate-data: generate-data.cpp
	$(CC) -o generate-data generate-data.cpp

example-data-10: generate-data
	./generate-data 1337 10000000 > example-data-10

example-data-50: generate-data
	./generate-data 1337 50000000 > example-data-50

clean-example-data:
	$(RM) example-data-10
	$(RM) example-data-50
