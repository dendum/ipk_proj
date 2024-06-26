
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
#include <vector>
#include <sstream>
#include <atomic>
#include <csignal>

using namespace std;

#define BUFFER_SIZE 1024

enum class Protocol {
    TCP,
    UDP,
    None
};

enum class IPKState {
    START,
    AUTH,
    OPEN,
    ERROR,
    BYE
};

enum class MESSAGEType {
    REPLY,
    MSG,
    ERR_MSG,
    ERR,
    AUTH,
    JOIN,
    UNKNOWN,
    BYE
};

/**
 * @class IPKClient
 * @brief Represents a client for the IPK messaging system
 */
class IPKClient {
    int port;
    int mode;
    string hostname;

    struct hostent* host;
    struct sockaddr_in server_address {};
    socklen_t addr_len = sizeof(server_address);

    string username;
    string displayName;
    string secret;

public:
    int fd;
    IPKState state;
    string err_msg;

    IPKClient(int port, string hostname, int protocol);
    ~IPKClient();

    void connect();
    void send_info(MESSAGEType messageType, const vector<string>& words);
    ssize_t send(const string& str);
    void receive(MESSAGEType messageType,const vector<string>& words);
    void clientPrint(MESSAGEType type, const vector<string>& messageContent, const string& sender);
    void rename(const vector<string>& words);
};


#endif //IPK_PROJ_IPKCLIENT_H
