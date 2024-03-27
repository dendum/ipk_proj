#include <iostream>
#include <csignal>
#include <cstdlib>
#include <atomic>
#include <cstdio>
#include <cstring>

using namespace std;

const int DEFAULT_PORT = 4567;
const int DEFAULT_TIMEOUT = 250;
const int DEFAULT_RETRANSMITS = 3;
const std::string USAGE_STRING = "Usage: ./ipk24 -s <host> -p <port> -t <mode> -d <timeout> -r <udpRet> -h <help>\n";
enum class Protocol {
    TCP,
    UDP,
    None
};

Protocol to_protocol(const std::string& str) {
    // Helper function
    if (str == "tcp") return Protocol::TCP;
    if (str == "udp") return Protocol::UDP;
    return Protocol::None;
}

void parse_arguments(int argc, char *argv[], std::string& hostname, Protocol& protocol, int& port, int& udp_timeout, int& max_retransmits) {
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

    return 0;
}
