#include "IPKClient.h"

IPKClient::IPKClient(int port, string hostname, int mode) {
    state = IPKState::START;

    /* Set attributes */
    this->port = port;
    this->hostname = std::move(hostname);
    this->mode = mode;

    /* Verify port number */
    if (this->port < 1 || this->port > 65535) {
        err_msg = "Invalid port!";
        state = IPKState::ERROR;
        return;
    }

    /* Resolve hostname */
    host = gethostbyname(this->hostname.c_str());
    if (host == nullptr) {
        err_msg = "Failed to resolve hostname!";
        state = IPKState::ERROR;
        return;
    }

    /* Create socket */
    fd = socket(AF_INET, this->mode, 0);
    if (fd < 0) {
        err_msg = "Failed to create socket!";
        this->state = IPKState::ERROR;
        return;
    }

    memset((&server_address), 0, this->addr_len);
    server_address.sin_family = AF_INET;
    memcpy(host->h_addr, (&(server_address).sin_addr.s_addr), host->h_length);
    server_address.sin_port = htons(this->port);
    int valid = inet_pton(AF_INET, this->hostname.c_str(), &server_address.sin_addr);
    if (valid <= 0) {
        err_msg = valid == 0 ? "Invalid address!" : "Failed to convert address!";
        state = IPKState::ERROR;
    }

}

void IPKClient::connect() {
    if (mode == SOCK_DGRAM) {
        // TODO: UDP
    }

    if (::connect(fd, (struct sockaddr*)&server_address, addr_len) == -1) {
        err_msg = "Failed to connect to server.";
        state = IPKState::ERROR;
        return;
    }
    this->state = IPKState::AUTH;
}

IPKClient::~IPKClient() {
    // TODO if (need) {disconnect()}
    shutdown(fd, SHUT_RDWR);
    close(fd);
}
