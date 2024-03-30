
#ifndef IPK_PROJ_IPKCLIENT_H
#define IPK_PROJ_IPKCLIENT_H

#include <iostream>
#include <cstring>
#include <cstdio>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <algorithm>
#include <unistd.h>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <sys/epoll.h>


using namespace std;

#define TIMEOUT 4

enum class IPKState {
    START,
    AUTH,
    OPEN,
    ERROR,
};

class IPKClient {
    int port;
    int mode;
    string hostname;
    struct hostent* host;
    struct sockaddr_in server_address {};
    socklen_t addr_len = sizeof(server_address);



public:
    int fd;
    IPKState state;
    string err_msg;

    IPKClient(int port, string hostname, int protocol);
    ~IPKClient();

    void connect();

//    void sendRequest();
//
//    void receiveResponse();
//
//    void disconnect();
};


#endif //IPK_PROJ_IPKCLIENT_H
