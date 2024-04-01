#define MAX_EVENTS 10

#include "IPKClient.h"

int pipefd[2];
//  a61b7fb9-f7e0-4d7d-b876-d26e8fcbc308
const int DEFAULT_PORT = 4567;
const int DEFAULT_TIMEOUT = 250;
const int DEFAULT_RETRANSMITS = 3;
const std::string USAGE_STRING = "Usage: ./ipk24 -s <host> -p <port> -t <mode> -d <timeout> -r <udpRet> -h <help>\n";
const std::string HELP_STRING = "help info:\n/auth\t{Username} {Secret} {DisplayName}\n/join\t{ChannelID}\n/rename\t{DisplayName}\n/help\n";

/**
 * @brief Converts a string to a Protocol.
 *
 * This function takes a string as input and returns the corresponding Protocol
 * value.
 *
 * @param str The input string to convert to a Protocol.
 * @return The corresponding Protocol value.
 */
Protocol to_protocol(const std::string &str) {
    if (str == "tcp") return Protocol::TCP;
    if (str == "udp") return Protocol::UDP;
    return Protocol::None;
}

/**
 * @brief Adds a file descriptor to an epoll instance.
 *
 * This function adds a file descriptor to the specified epoll instance using
 * the epoll_ctl system call with the EPOLL_CTL_ADD operation.
 *
 * @param epoll_fd The file descriptor referring to the epoll instance.
 * @param event The epoll_event structure that contains the event data.
 * @param fd The file descriptor to be added to the epoll instance.
 */
void epoll_ctl_add(int epoll_fd, struct epoll_event &event, int fd) {
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
        std::cerr << "Failed to add to epoll." << std::endl;
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Signal handler for SIGINT
 */
void handle_sigint(int sig) {
    write(pipefd[1], "X", 1);
}

/**
 * @brief Cleanup function to close file descriptors.
 *
 * This function closes the read and write ends of a pipe and an epoll file descriptor.
 *
 * @param pipefd An array containing the read and write ends of the pipe.
 * @param epoll_fd The epoll file descriptor to be closed.
 */
void cleanup(int pipefd[], int epoll_fd) {
    close(pipefd[0]);
    close(pipefd[1]);
    close(epoll_fd);
}

/**
 * @brief Parses the command line arguments and assigns the values to the corresponding variables.
 *
 * This function parses the command line arguments using getopt and assigns the values to the
 * specified variables. The allowed options are -t, -s, -p, -d, -r, and -h.
 *
 * @param argc The total number of command line arguments.
 * @param argv An array of strings containing the command line arguments.
 * @param hostname A string reference to store the value of the -s option.
 * @param protocol A Protocol reference to store the value of the -t option.
 * @param port An integer reference to store the value of the -p option.
 * @param udp_timeout An integer reference to store the value of the -d option.
 * @param max_retransmits An integer reference to store the value of the -r option.
 */
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
                exit(EXIT_SUCCESS);
            default:
                break;
        }
    }
}

/**
 * @brief Configures a new IPKClient object.
 *
 * This function creates a new IPKClient object with the specified hostname, protocol, and port number.
 * The IPKClient object is configured based on the specified protocol and connects to the specified hostname and port.
 *
 * @param hostname The hostname to connect to.
 * @param protocol The protocol to use (TCP or UDP).
 * @param port The port number to connect to.
 * @return The configured IPKClient object.
 */
IPKClient ConfigureClient(const std::string &hostname, Protocol protocol, int port) {
    IPKClient client(port, hostname, protocol == Protocol::TCP ? SOCK_STREAM : SOCK_DGRAM);
    if (client.state == IPKState::ERROR) {
        cerr << "ERR: " << client.err_msg << endl;
        exit(EXIT_FAILURE);
    }
    client.connect();
    if (client.state == IPKState::ERROR) {
        cerr << "ERR: " << client.err_msg << endl;
        exit(EXIT_FAILURE);
    }
    return client;
}

vector<string> getInputData(const string &str) {
    std::vector<std::string> words;
    std::stringstream ss(str);
    std::string word;
    while (ss >> word)
        words.push_back(word);
    return words;
}

/**
 * @brief Check the client state and break if necessary.
 *
 * @param clientState The current state of the client.
 * @param going A boolean reference representing whether the program should continue or break.
 */
void checkStateAndBreakIfNecessary(IPKState clientState, bool &going) {
    if (clientState == IPKState::BYE || clientState == IPKState::ERROR) {
        going = false;
    }
}

int main(int argc, char *argv[]) {
    std::string hostname;
    Protocol protocol = Protocol::None;
    int port = DEFAULT_PORT;
    int udp_timeout = DEFAULT_TIMEOUT;
    int max_retransmits = DEFAULT_RETRANSMITS;

    parse_arguments(argc, argv, hostname, protocol, port, udp_timeout, max_retransmits);

    // Check for errors and print usage if necessary
    if (hostname.empty() || protocol == Protocol::None) {
        cerr << "ERR: " << (hostname.empty() ? "Hostname" : "Mode") << " not specified!\n" << USAGE_STRING;
        return EXIT_FAILURE;
    }

    IPKClient client = ConfigureClient(hostname, protocol, port);

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
        for (int i = 0; i < num_events; ++i) {
            if (events[i].data.fd == stdin_fd) {
                std::cin.getline(buffer, BUFFER_SIZE);
                if (std::cin.eof()) {
                    cleanup(pipefd, epoll_fd);
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
                        client.clientPrint(MESSAGEType::ERR, {"You are already authed!"}, "");
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
                checkStateAndBreakIfNecessary(client.state, going);
                if (!going)
                    break;
                memset(buffer, 0, BUFFER_SIZE);
            } else if (events[i].data.fd == client.fd) {
                int bytes_received = recv(client.fd, buffer, BUFFER_SIZE, 0);
                if (bytes_received == 0) {
                    client.clientPrint(MESSAGEType::ERR, {"Server closed the connection."}, "");
                    client.send_info(MESSAGEType::BYE, {"BYE"});
                    cleanup(pipefd, epoll_fd);
                    return EXIT_FAILURE;
                }
                buffer[bytes_received] = '\0';
                vector<string> words = getInputData(std::string(buffer));
                if (words.empty())
                    continue;
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
                    } else {
                        client.receive(MESSAGEType::UNKNOWN, words);
                    }
                }
                checkStateAndBreakIfNecessary(client.state, going);
                if (!going)
                    break;
                memset(buffer, 0, BUFFER_SIZE);
            } else if (events[i].data.fd == pipefd[0]) {
                client.send_info(MESSAGEType::BYE, {"BYE"});
                checkStateAndBreakIfNecessary(client.state, going);
                if (!going)
                    break;
            }
        }
    }
    if (client.state == IPKState::ERROR) {
        client.clientPrint(MESSAGEType::ERR, {client.err_msg}, "");
        cleanup(pipefd, epoll_fd);
        return EXIT_FAILURE;
    }

    cleanup(pipefd, epoll_fd);
    return 0;

}
