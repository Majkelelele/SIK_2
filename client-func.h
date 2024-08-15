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
void send_IAM(int socket_fd, char place, char *ip_sender,
 uint16_t port_sender, char *ip_local, uint16_t port_local);
std::vector<Card> read_deal(int socket_fd, char *ip_sender,
 uint16_t port_sender, char *ip_local, uint16_t port_local);
void read_score(int socket_fd, char *ip_sender,
 uint16_t port_sender, char *ip_local, uint16_t port_local);
void read_taken(int socket_fd, const std::string &ip_sender, uint16_t port_sender, 
                const std::string &ip_local, uint16_t port_local);
void read_total(int socket_fd, char *ip_sender,
 uint16_t port_sender, char *ip_local, uint16_t port_local);
Card choose_card(std::string card_list, std::vector<Card> &remaining_cards);
struct sockaddr_in get_server_address(char const *host, uint16_t port);

#endif
