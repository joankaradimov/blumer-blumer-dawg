# the compiler: gcc for C program, define as g++ for C++
CC = g++

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall --std=c++11 -O2
DEFINES = -DNDEBUG

# the build target executable:
TARGET = blumer-blumer

all: $(TARGET)

$(TARGET): $(TARGET).cpp
	$(CC) $(CFLAGS) $(DEFINES) -o $(TARGET) $(TARGET).cpp

clean:
	$(RM) $(TARGET)
	$(RM) generate-data

generate-data: generate-data.cpp
	$(CC) -o generate-data generate-data.cpp

example-data: generate-data
	./generate-data 1337 50000000 > example-data

clean-example-data:
	$(RM) example-data
