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
        case IPKState::BYE:
            return "BYE";
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
    if (host == NULL) {
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
    memcpy((&(server_address).sin_addr.s_addr), host->h_addr, host->h_length);
    server_address.sin_port = htons(this->port);

}

void IPKClient::connect() {
    if (mode == SOCK_DGRAM) {
        // TODO: UDP
    }

    if (::connect(fd, (struct sockaddr *) &server_address, addr_len) == -1) {
        err_msg = "Failed to connect to server.";
        state = IPKState::ERROR;
        return;
    }
    this->state = IPKState::AUTH;
}

void IPKClient::disconnect() {
    // TCP
    if (mode == SOCK_STREAM) {
        /* Client is up - graceful exit */
        if (state != IPKState::BYE) {

        }

    }
}

IPKClient::~IPKClient() {
    // TODO if (need) {disconnect()}
    shutdown(fd, SHUT_RDWR);
    close(fd);
}

void IPKClient::send_info(MESSAGEType messageType, const vector<string> &words) {
    if (messageType == MESSAGEType::AUTH) {
        if (words.size() != 4) { // TODO Add checkers
            clientPrint(MESSAGEType::ERR, {"Invalid /auth data! Try again!"}, "");
            return;
        }

        username = words[1];
        secret = words[2];
        displayName = words[3];

        int sent_bytes = send("AUTH " + username + " AS " + displayName + " USING " + secret + "\r\n");
        if (sent_bytes < 0) {
            err_msg = "Failed to send all data to server!";
            this->state = IPKState::ERROR;
        }
    } else if (messageType == MESSAGEType::MSG) {
        string str = "MSG FROM " + displayName + " IS ";
        for (const auto &word: words) {
            str += word + " ";
        }
        str = str.substr(0, str.size() - 1);
        str += "\r\n";

        int sent_bytes = send(str);
        if (sent_bytes < 0) {
            err_msg = "Failed to send all data to server!";
            this->state = IPKState::ERROR;
        }
    } else if (messageType == MESSAGEType::ERR_MSG) {
        string str = "ERR FROM " + displayName + " IS ";
        for (const auto &word: words) {
            str += word + " ";
        }
        str = str.substr(0, str.size() - 1);
        str += "\r\n";

        int sent_bytes = send(str);
        if (sent_bytes < 0) {
            err_msg = "Failed to send all data to server!";
            this->state = IPKState::ERROR;
        }
    } else if (messageType == MESSAGEType::JOIN) {
        if (words.size() != 2) { // TODO Add checkers
            clientPrint(MESSAGEType::ERR, {"Invalid /join data! Try again!"}, "");
            return;
        }

        string channelID = words[1];

        int sent_bytes = send("JOIN " + channelID + " AS " + displayName + "\r\n");
        if (sent_bytes < 0) {
            err_msg = "Failed to send all data to server!";
            this->state = IPKState::ERROR;
        }
    } else if (messageType == MESSAGEType::BYE) {
        if (words.size() != 1) { // TODO Add checkers
            clientPrint(MESSAGEType::ERR, {"Invalid \"BYE\" message format! Try again!"}, "");
            return;
        }

        state = IPKState::BYE;
        int sent_bytes = send("BYE\r\n");
        if (sent_bytes < 0) {
            err_msg = "Failed to send all data to server!";
            this->state = IPKState::ERROR;
        }
    }
}

ssize_t IPKClient::send(const string &str) {
    array<char, BUFFER_SIZE> buffer{0};
    memcpy(buffer.data(), str.data(), str.size() + 1);

    ssize_t sent_bytes = ::send(fd, buffer.data(), str.size(), 0);
    return sent_bytes;
}

void IPKClient::receive(MESSAGEType messageType, const vector<string> &words) {
    if (messageType == MESSAGEType::REPLY) {
        if (words.size() > 2) {
            std::string replyStatus = words[1];
            std::vector<std::string> messageContent(words.begin() + 3, words.end());
            if (state == IPKState::AUTH && replyStatus == "OK") {
                state = IPKState::OPEN;
            }
            clientPrint(MESSAGEType::REPLY, messageContent, replyStatus);
        } else {
            messageType = MESSAGEType::UNKNOWN;
        }
    } else if (messageType == MESSAGEType::MSG) {
        if (words.size() > 3) {
            std::string senderDName = words[2];
            std::vector<std::string> messageContent(words.begin() + 4, words.end());
            clientPrint(MESSAGEType::MSG, messageContent, senderDName);
        } else {
            messageType = MESSAGEType::UNKNOWN;
        }
    } else if (messageType == MESSAGEType::ERR_MSG) {
        if (words.size() > 3) {
            std::string senderDName = words[2];
            std::vector<std::string> messageContent(words.begin() + 4, words.end());
            clientPrint(MESSAGEType::ERR_MSG, messageContent, senderDName);
            send_info(MESSAGEType::BYE, {"BYE"});
        } else {
            messageType = MESSAGEType::UNKNOWN;
        }
    }

    if (messageType == MESSAGEType::UNKNOWN) {
        clientPrint(MESSAGEType::ERR, {"Invalid message from server!"}, "");
        send_info(MESSAGEType::ERR_MSG, {"Invalid message from server!"});
        send_info(MESSAGEType::BYE, {"BYE"});
        state = IPKState::BYE;
    }
}

void IPKClient::clientPrint(MESSAGEType type, const vector<string> &messageContent, const string &sender) {
    switch (type) {
        case MESSAGEType::REPLY: {
            if (sender == "OK") {
                cerr << "Success: ";
            } else {
                cerr << "Failure: ";
            }
            for (const auto &message: messageContent) {
                cerr << message;
                if (&message != &messageContent.back()) {
                    cerr << " ";
                }
            }
            cerr << "\n";
            break;
        }
        case MESSAGEType::MSG:
            cout << sender << ": ";
            for (const auto &message: messageContent) {
                cout << message;
                if (&message != &messageContent.back()) {
                    cout << " ";
                }
            }
            cout << "\n";
            break;
        case MESSAGEType::ERR_MSG:
            cerr << "ERR FROM " << sender << ": ";
            for (const auto &message: messageContent) {
                cerr << message;
                if (&message != &messageContent.back()) {
                    cerr << " ";
                }
            }
            cerr << "\n";
            break;
        case MESSAGEType::ERR:
            cerr << "ERR: ";
            for (const auto &message: messageContent) {
                cerr << message;
                if (&message != &messageContent.back()) {
                    cerr << " ";
                }
            }
            cerr << "\n";
            break;
        case MESSAGEType::AUTH:
            break;
        case MESSAGEType::JOIN:
            break;
        case MESSAGEType::BYE:
            break;
        case MESSAGEType::CONFIRM:
            break;
        case MESSAGEType::UNKNOWN:
            break;
    }
}

void IPKClient::rename(const vector<string> &words) {
    if (words.size() != 2) { // TODO Add checkers
        clientPrint(MESSAGEType::ERR, {"Invalid /rename data! Try again!"}, "");
        return;
    }
    displayName = words[1];
}

