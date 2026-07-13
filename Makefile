CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread

TARGET = synapse
SRCS = main.cpp ai_agent.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) -lcurl -lreadline

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
