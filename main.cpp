#define MAX_EVENTS 10

#include "IPKClient.h"

int pipefd[2];
// a61b7fb9-f7e0-4d7d-b876-d26e8fcbc308
const int DEFAULT_PORT = 4567;
const int DEFAULT_TIMEOUT = 250;
const int DEFAULT_RETRANSMITS = 3;
const std::string USAGE_STRING = "Usage: ./ipk24 -s <host> -p <port> -t <mode> -d <timeout> -r <udpRet> -h <help>\n";
const std::string HELP_STRING = "help info:\n/auth\t{Username} {Secret} {DisplayName}\n/join\t{ChannelID}\n/rename\t{DisplayName}\n/help\n";
enum class Protocol {
    TCP,
    UDP,
    None
};

Protocol to_protocol(const std::string &str) {
    // Helper function
    if (str == "tcp") return Protocol::TCP;
    if (str == "udp") return Protocol::UDP;
    return Protocol::None;
}

void epoll_ctl_add(int epoll_fd, struct epoll_event &event, int fd) {
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        std::cerr << "Failed to add to epoll." << std::endl;
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }
}

void parse_arguments(int argc, char *argv[], std::string &hostname, Protocol &protocol, int &port, int &udp_timeout,
                     int &max_retransmits) {
    int option;
    while ((option = getopt(argc, argv, "t:s:p:d:r:h")) != -1) {
        switch (option) {
            case 't':
                protocol = to_protocol(optarg);
                break;
            case 's':
                hostname = optarg;
                break;
            case 'p':
                port = std::stoi(optarg);
                break;
            case 'd':
                udp_timeout = std::stoi(optarg);
                break;
            case 'r':
                max_retransmits = std::stoi(optarg);
                break;
            case 'h':
                cout << USAGE_STRING;
                exit(EXIT_FAILURE);
            default:
                break;
        }
    }
}

vector<string> getInputData(const string &str) {
    std::vector<std::string> words;
    std::stringstream ss(str);
    std::string word;
    while (ss >> word)
        words.push_back(word);
    return words;
}

// Signal handler
void handle_sigint(int sig) {
    write(pipefd[1], "X", 1); // write to pipe
}

int main(int argc, char *argv[]) {

    std::string hostname;
    Protocol protocol = Protocol::None;
    int port = DEFAULT_PORT;
    int udp_timeout = DEFAULT_TIMEOUT;
    int max_retransmits = DEFAULT_RETRANSMITS;

    parse_arguments(argc, argv, hostname, protocol, port, udp_timeout, max_retransmits);
//    cout << port << endl;
//    cout << hostname << endl;
//    cout << static_cast<int>(protocol) << endl;

    // Check for errors and print usage if necessary
    if (hostname.empty() || protocol == Protocol::None) {
        cerr << "ERR: " << (hostname.empty() ? "Hostname" : "Mode") << " not specified!\n" << USAGE_STRING;
        return EXIT_FAILURE;
    }

    IPKClient client(port, hostname, protocol == Protocol::TCP ? SOCK_STREAM : SOCK_DGRAM);
    if (client.state == IPKState::ERROR) {
        cerr << "ERR: " << client.err_msg << endl;
        return EXIT_FAILURE;
    }

    /*Connect to server*/
    client.connect();
    if (client.state == IPKState::ERROR) {
        cerr << "ERR: " << client.err_msg << endl;
        return EXIT_FAILURE;
    }

    // Set non-blocking mode for stdin
    int stdin_fd = fileno(stdin);
    int flags = fcntl(stdin_fd, F_GETFL, 0);
    fcntl(stdin_fd, F_SETFL, flags | O_NONBLOCK);

    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        std::cerr << "Failed to create epoll instance." << std::endl;
        close(client.fd);
        return 1;
    }

    // setup signal handler and create pipe
    signal(SIGINT, handle_sigint);
    pipe(pipefd);
    // Add stdin and client socket to epoll
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl_add(epoll_fd, event, client.fd);
    epoll_ctl_add(epoll_fd, event, stdin_fd);
    epoll_ctl_add(epoll_fd, event, pipefd[0]);

    char buffer[BUFFER_SIZE];
    bool going = true;
    while (going) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
//        cout << "CUR STATE: " << client.getState() << endl;
        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == stdin_fd) {
                std::cin.getline(buffer, BUFFER_SIZE);
                if (std::cin.eof()) {
                    close(pipefd[0]);
                    close(pipefd[1]);
                    close(epoll_fd);
                    return 0;
                }
                vector<string> words = getInputData(std::string(buffer));
                if (words.empty())
                    continue;
                if (client.state == IPKState::AUTH) {
                    if (words[0] == "/auth") {
                        client.send_info(MESSAGEType::AUTH, words);
                    } else if (words[0] == "/help") {
                        cout << HELP_STRING;
                    } else {
                        client.clientPrint(MESSAGEType::ERR,
                                           {"You are not authed! Try: /auth {Username} {Secret} {DisplayName}"}, "");
                    }
                } else if (client.state == IPKState::OPEN) {
                    if (words[0] == "/auth") {
                        client.clientPrint(MESSAGEType::ERR,{"You are already authed!"}, "");
                    } else if (words[0] == "/join") {
                        client.send_info(MESSAGEType::JOIN, words);
                    } else if (words[0] == "/rename") {
                        client.rename(words);
                    } else if (words[0] == "/help") {
                        cout << HELP_STRING;
                    } else if (words[0] == "BYE") {
                        client.send_info(MESSAGEType::BYE, words);
                    } else {
                        client.send_info(MESSAGEType::MSG, words);
                    }
                }
                if (client.state == IPKState::BYE || client.state == IPKState::ERROR) {
                    going = false;
                    break;
                }

                memset(buffer, 0, BUFFER_SIZE);
            } else if (events[i].data.fd == client.fd) {
                int bytes_received = recv(client.fd, buffer, BUFFER_SIZE, 0);
                if (bytes_received <= 0) {
                    client.clientPrint(MESSAGEType::ERR, {"Server closed the connection."}, "");
//                    client.send_info(MESSAGEType::BYE, {"BYE"});
                    close(pipefd[0]);
                    close(pipefd[1]);
                    close(epoll_fd);
                    return EXIT_FAILURE;
                }
                buffer[bytes_received] = '\0';
                vector<string> words = getInputData(std::string(buffer));
                if (client.state == IPKState::AUTH) {
                    if (words[0] == "REPLY") {
                        client.receive(MESSAGEType::REPLY, words);
                    } else if (words[0] == "MSG") {
                        client.receive(MESSAGEType::MSG, words);
                    } else if (words[0] == "ERR") {
                        client.receive(MESSAGEType::ERR_MSG, words);
                    }
                } else if (client.state == IPKState::OPEN) {
                    if (words[0] == "REPLY") {
                        client.receive(MESSAGEType::REPLY, words);
                    } else if (words[0] == "MSG") {
                        client.receive(MESSAGEType::MSG, words);
                    } else if (words[0] == "ERR") {
                        client.receive(MESSAGEType::ERR_MSG, words);
                    } else if (words[0] == "BYE") {
                        client.state = IPKState::BYE;
                    } else {
                        client.receive(MESSAGEType::UNKNOWN, words);
                    }
                }
                if (client.state == IPKState::BYE || client.state == IPKState::ERROR) {
                    going = false;
                    break;
                }
                memset(buffer, 0, BUFFER_SIZE);
            } else if(events[i].data.fd == pipefd[0]) {
                client.send_info(MESSAGEType::BYE, {"BYE"});
                if (client.state == IPKState::BYE || client.state == IPKState::ERROR) {
                    going = false;
                    break;
                }
            }
        }
    }
    if (client.state == IPKState::ERROR) {
        client.clientPrint(MESSAGEType::ERR, {client.err_msg}, "");
        return EXIT_FAILURE;
    }

    // Cleanup
    close(pipefd[0]);
    close(pipefd[1]);
    close(epoll_fd);
    return 0;

}
