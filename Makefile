CC := g++
CFLAGS := -std=c++11 -Wall -Wextra
SRCS = main.cpp IPKClient.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = ipk24chat-client

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
