#define MAX_EVENTS 10

#include "IPKClient.h"

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

int main(int argc, char *argv[]) {
    std::cout << "Hello, World!" << std::endl;

    std::string hostname;
    Protocol protocol = Protocol::None;
    int port = DEFAULT_PORT;
    int udp_timeout = DEFAULT_TIMEOUT;
    int max_retransmits = DEFAULT_RETRANSMITS;

    parse_arguments(argc, argv, hostname, protocol, port, udp_timeout, max_retransmits);

    // Check for errors and print usage if necessary
    if (hostname.empty() || protocol == Protocol::None) {
        cerr << "ERROR: " << (hostname.empty() ? "Hostname" : "Mode") << " not specified!\n" << USAGE_STRING;
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

    // Add stdin and client socket to epoll
    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl_add(epoll_fd, event, client.fd);
    epoll_ctl_add(epoll_fd, event, stdin_fd);

    char buffer[BUFFER_SIZE];

    while (true) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        cout << "CUR STATE: " << client.getState() << endl;
        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == stdin_fd) {
                std::cin.getline(buffer, BUFFER_SIZE);
                if (std::cin.eof()) {
                    close(epoll_fd);
                    return 0;
                }
                vector<string> words = getInputData(std::string(buffer));
                if (words.empty())
                    continue;
                if (client.state == IPKState::AUTH) {
                    if (words[0] == "/auth") {
                        if (!client.send_auth_info(words)) {
                            client.printError();
                        } else if (client.state == IPKState::ERROR) {
                            client.printError();
                            return EXIT_FAILURE;
                        }
                    } else if (words[0] == "/help") {
                        cout << HELP_STRING;
                    } else {
                        // TODO rest
                        client.err_msg = "You are not authed! PS: TODO rest";
                        client.printError();
                    }
                } else if (client.state == IPKState::OPEN) {
                    if (words[0] == "/join") {
                    } else if (words[0] == "/rename") {
                    } else if (words[0] == "/help") {
                        cout << HELP_STRING;
                    } else {
                        if (client.send_info(MESSAGEType::MSG, words)) {
                            // TODO ERROR
                        }
                    }
                } else if (client.state == IPKState::ERROR) {

                } else {

                }

                memset(buffer, 0, BUFFER_SIZE);
            } else if (events[i].data.fd == client.fd) {
                int bytes_received = recv(client.fd, buffer, BUFFER_SIZE, 0);
                if (bytes_received <= 0) {
                    cerr << "Server closed the connection." << std::endl;
                    close(epoll_fd);
                    return EXIT_FAILURE;
                }
                buffer[bytes_received] = '\0';
                vector<string> words = getInputData(std::string(buffer));
                if (client.state == IPKState::AUTH) {
                    if (words[0] == "REPLY") {
                        client.receive(MESSAGEType::REPLY, words);
                    } else {
                        // something else maybe/ 99.9%
                    }
                } else if (client.state == IPKState::OPEN) {
                    if (words[0] == "REPLY") {
                        client.receive(MESSAGEType::REPLY, words);
                    } else {

                    }
                    if (words[0] == "MSG") {
                        client.receive(MESSAGEType::MSG, words);
                    } else {

                    }
                } else if (client.state == IPKState::ERROR) {

                } else {

                }

//                cout << "Received: " << buffer;
                memset(buffer, 0, BUFFER_SIZE);
            }
        }
    }

    // Cleanup
    close(epoll_fd);
    return 0;

}
