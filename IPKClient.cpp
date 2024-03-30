#include "IPKClient.h"

string IPKClient::getState() {
    switch (state) {
        case IPKState::START:
            return "START";
        case IPKState::AUTH:
            return "AUTH";
        case IPKState::OPEN:
            return "OPEN";
        case IPKState::ERROR:
            return "ERROR";
    }
    return "NONE";
}


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

bool IPKClient::send_auth_info(const vector<string>& words) {
    if (words.size() != 4) { // TODO Add checkers
        err_msg = "Invalid /auth data! Try again!";
        return false;
    }

    username = words[1];
    secret = words[2];
    displayName = words[3];

    int sent_bytes = send("AUTH " + username + " AS " + displayName + " USING " + secret + "\r\n");
    if (sent_bytes < 0) {
        err_msg = "Failed to send all data to server!";
        this->state = IPKState::ERROR;
    }
    return true;
}

ssize_t IPKClient::send(const string &str) {
    array<char, BUFFER_SIZE> buffer{0};
    memcpy(buffer.data(), str.data(), str.size() + 1);

    ssize_t sent_bytes = ::send(fd, buffer.data(), str.size(), 0);
    if (sent_bytes < 0) {
        err_msg = "Failed to send data to server!";
        this->state = IPKState::ERROR;
    }

    return sent_bytes;
}

void IPKClient::receive(MESSAGEType messageType, const vector<string>& words) {
    if (messageType == MESSAGEType::REPLY) {
        if(words.size() > 2) {
            std::string replyStatus = words[1];
            std::vector<std::string> messageContent(words.begin()+3, words.end());
            if(replyStatus == "OK") {
                state = IPKState::OPEN;
            }
            clientPrint(MESSAGEType::REPLY, messageContent);
        } else {
            err_msg = "Invalid message format!";
            this->state = IPKState::ERROR;
            // TODO SEND ERR AND EXIT
        }
    } else {

    }
}

void IPKClient::clientPrint(MESSAGEType type, const vector<string>& messageContent) {
    switch (type) {
        case MESSAGEType::REPLY:{
            if (state == IPKState::OPEN) {
                cout << "Success: ";
            } else {
                cout << "Failure: ";
            }
            for (const auto &message: messageContent) {
                cout << message << " ";
            }
            cout << "\n";
            break;
        }
        case MESSAGEType::ERR:
            break;
        case MESSAGEType::AUTH:
            break;
        case MESSAGEType::JOIN:
            break;
        case MESSAGEType::MSG:
            break;
        case MESSAGEType::BYE:
            break;
        case MESSAGEType::CONFIRM:
            break;
    }
}

void IPKClient::printError() {
    cerr << "ERR: " << err_msg << endl;
}

