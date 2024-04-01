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





#CC := g++
#CFLAGS := -std=c99 -Wall -Wextra
#SRCS = chat_handler.cpp Comm.cpp main.cpp packets.cpp packets_tcp.cpp
#OBJS = chat_handler.o Comm.o main.o packets.o packets_tcp.o
#TARGET = ipk24chat-client
#
#$(TARGET): $(OBJS)
#    $(CC) -o $@ $^
#
#%.o: %.c
#    $(CC) $(CFLAGS) -c $< -o $@
#
#clean:
#    rm -f $(OBJS) $(TARGET)
