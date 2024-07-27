#ifndef CLIENT_FUNC_H
#define CLIENT_FUNC_H

#include <string>
#include "common.h"

// #define BUFFER_SIZE 1000
struct ClientParams {
    std::string host;
    int port;
    bool ipv4;
    bool ipv6;
    char place;
    bool automatic;
};

void printUsage();
ClientParams parseArgumentsClient(int argc, char* argv[]);
void send_IAM(int socket_fd, char place);
std::vector<std::string> read_deal(int socket_fd);

#endif
