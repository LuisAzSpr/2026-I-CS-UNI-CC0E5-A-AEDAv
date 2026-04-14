CXX = g++
CXXFLAGS = -std=c++11 -Wall -g

TARGET = main.exe
SRCS = main.cpp \
        containers/vector.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Versión para Windows
clean:
	del /f /q main.o containers\vector.o $(TARGET)

.PHONY: all clean