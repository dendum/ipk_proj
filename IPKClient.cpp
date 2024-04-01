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

/**
 * @brief Establishes a connection with the server
 *
 * This function is used to establish a connection with the server.
 *
 * @note The function assumes that the necessary variables (fd, server_address, addr_len, err_msg, state) have been properly initialized.
 */
void IPKClient::connect() {
    if (mode == SOCK_DGRAM) {
        // UDP
        err_msg = "Failed to connect to server.";
        state = IPKState::ERROR;
        return;
    }

    if (::connect(fd, (struct sockaddr *) &server_address, addr_len) == -1) {
        err_msg = "Failed to connect to server.";
        state = IPKState::ERROR;
        return;
    }
    this->state = IPKState::AUTH;
}

IPKClient::~IPKClient() {
    shutdown(fd, SHUT_RDWR);
    close(fd);
}

/**
 * @brief Sends information to the server based on the specified message type and words
 *
 * This function is responsible for sending information to the server based on the specified message type and words.
 * It first checks the message type and performs the necessary actions.
 *
 * @param messageType The type of the message to send
 * @param words The words associated with the message
 */
void IPKClient::send_info(MESSAGEType messageType, const vector<string> &words) {
    if (messageType == MESSAGEType::AUTH) {
        if (words.size() != 4) {
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
        if (words.size() != 2) {
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
        if (words.size() != 1) {
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

/**
 * @brief Sends a string message to the server.
 *
 * The function sends the provided string message to the server using the initialized socket file descriptor (fd).
 * The message is copied into a buffer and then sent using the send() function.
 *
 * @param str The string message to be sent.
 * @return The number of bytes sent on success, or -1 on error.
 */
ssize_t IPKClient::send(const string &str) {
    array<char, BUFFER_SIZE> buffer{0};
    memcpy(buffer.data(), str.data(), str.size() + 1);

    ssize_t sent_bytes = ::send(fd, buffer.data(), str.size(), 0);
    return sent_bytes;
}

/**
 * @brief Receives a message from the server and performs the necessary actions based on the message type and words
 *
 * @param messageType The type of the message received
 * @param words The words associated with the message
 */
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
        sleep(1);
        send_info(MESSAGEType::BYE, {"BYE"});
        state = IPKState::BYE;
    }
}

/**
 * @brief Prints the message content based on the message type and the sender.
 *
 * This function prints the message content to standard output or standard error
 * based on the message type.
 *
 * @param type The type of the message.
 * @param messageContent The vector of message content.
 * @param sender The sender of the message.
 */
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
        default:
            break;
    }
}

/**
 * @brief Renames the display name of the client.
 *
 * @param words The vector of words containing the command name and the new display name.
 */
void IPKClient::rename(const vector<string> &words) {
    if (words.size() != 2) {
        clientPrint(MESSAGEType::ERR, {"Invalid /rename data! Try again!"}, "");
        return;
    }
    displayName = words[1];
}

