#include <iostream>
#include <vector>
#include <algorithm>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "err.h"
#include "client-func.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "err.h"
#include "common.h"
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>


void printUsage() {
    std::cerr << "Usage: client -h <host> -p <port> [-4|-6] -[NESW] [-a]\n";
}

ClientParams parseArgumentsClient(int argc, char* argv[]) {
    ClientParams params;
    params.ipv4 = false;
    params.ipv6 = false;
    params.automatic = false;

    std::vector<std::string> args(argv + 1, argv + argc);

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-h" && i + 1 < args.size()) {
            params.host = args[i + 1];
            ++i;
        } else if (args[i] == "-p" && i + 1 < args.size()) {
            params.port = read_port(args[i+1].c_str());
            ++i;
        } else if ((args[i] == "-4" || args[i] == "-6") && !params.ipv4 && !params.ipv6) {
            if (args[i] == "-4") {
                params.ipv4 = true;
            } else {
                params.ipv6 = true;
            }
        } else if (args[i] == "-N" || args[i] == "-E" || args[i] == "-S" || args[i] == "-W") {
            params.place = args[i][1];
        } else if (args[i] == "-a") {
            params.automatic = true;
        }
    }
    if (params.host.empty() || params.port == 0 || params.place == 0) {
        printUsage();
        exit(1);
    }

    return params;
}

void send_IAM(int socket_fd, char place, char *ip_sender,
 uint16_t port_sender, char *ip_local, uint16_t port_local) {
    static char line[BUFFER_SIZE];
    memset(line, 0, BUFFER_SIZE);
    snprintf(line, BUFFER_SIZE, "IAM%c\r\n", place);
    
    // Wysyłanie wiadomości
    size_t data_to_send = strnlen(line, BUFFER_SIZE);
    print_formatted_message(line, data_to_send, ip_local, port_local, ip_sender, port_sender);
    ssize_t written_length = writen(socket_fd, line, data_to_send);
    if (written_length < 0) {
        syserr("writen");
    } else if ((size_t) written_length != data_to_send) {
        fatal("incomplete writen");
    }
}

std::vector<Card> read_deal(int socket_fd, char *ip_sender,
 uint16_t port_sender, char *ip_local, uint16_t port_local) {
    char buffer[BUFFER_SIZE];
    ssize_t received_bytes = readn(socket_fd, buffer, BUFFER_SIZE);
    std::string card_list;
    std::vector<Card> cards;
    if (received_bytes < 0) {
        close(socket_fd);
        error("error when reading message from connection");
    } else if (received_bytes == 0) {
        close(socket_fd);
        error("ending connection\n");
    } else {
        print_formatted_message(buffer, received_bytes, ip_sender, port_sender, ip_local, port_local);
        
        buffer[received_bytes] = '\0';  
        std::string message(buffer);
        std::string deal_type;
        
        if (message.substr(0, 4) == "DEAL") {
            deal_type = message.substr(4, 1); // Typ rozdania to jednoliterowy string
            card_list = message.substr(6, message.length() - 8); // Reszta to lista kart, -3 aby usunąć \r\n
        } else if(message.substr(0, 5) == "TOTAL") { 
            return cards;
        } 
        else {
            error("Invalid message format. Expected message to start with 'DEAL'.");
        }
    }
    std::vector<std::string> sortedCards = parseCards(card_list);

    for(size_t i = 0; i < sortedCards.size(); i++) {
        cards.push_back(createCardFromString(sortedCards[i],"CLIENT"));
    }
    return cards;
}

void read_score(int socket_fd, char *ip_sender, uint16_t port_sender, char *ip_local, uint16_t port_local) {
    char buffer[BUFFER_SIZE];
    ssize_t received_bytes = readn(socket_fd, buffer, BUFFER_SIZE);

    if (received_bytes < 0) {
        close(socket_fd);
        error("error when reading message from connection");
    } else if(received_bytes == 0){
        close(socket_fd);
        syserr("ending connection\n");
    }
    else {
        print_formatted_message(buffer, received_bytes, ip_sender, port_sender, ip_local, port_local);
    }
}


void read_taken(int socket_fd, const std::string &ip_sender, uint16_t port_sender, 
                const std::string &ip_local, uint16_t port_local) {
    char buffer[BUFFER_SIZE];
    ssize_t received_bytes = readn(socket_fd, buffer, BUFFER_SIZE);

    if (received_bytes > 0) {
        print_formatted_message(buffer, received_bytes, ip_sender, port_sender, ip_local, port_local);
    } else {
        if (received_bytes < 0) {
            close(socket_fd);
            error("error when reading message from connection");
        } else {
            close(socket_fd);
            syserr("ending connection\n");
        }   
    }
}

void read_total(int socket_fd, char *ip_sender, uint16_t port_sender, char *ip_local, uint16_t port_local) {
    char buffer[BUFFER_SIZE];
    ssize_t received_bytes = readn(socket_fd, buffer, BUFFER_SIZE);

    if (received_bytes < 0) {
        close(socket_fd);
        error("error when reading message from connection");
    } else if(received_bytes == 0){
        close(socket_fd);
        syserr("ending connection\n");
    }
    else {
        print_formatted_message(buffer, received_bytes, ip_sender, port_sender, ip_local, port_local);
    }
}


Card choose_card(std::string card_list, std::vector<Card> &remaining_cards) {
    // Przekształć listę kart w wektor obiektów Card
    std::vector<std::string> current_cards = parseCards(card_list);
    std::vector<Card> cards;
    for (size_t i = 0; i < current_cards.size(); i++) {
        cards.push_back(createCardFromString(current_cards[i], "CLIENT"));
    }

    if (cards.empty()) {
        int random_index = 0;
        Card chosen_card = remaining_cards[random_index];
        remaining_cards.erase(remaining_cards.begin() + random_index); // Usuń wybraną kartę
        return chosen_card;
    }

    // Sprawdź kolor pierwszej karty
    std::string first_card_color = cards[0].getColor();

    // Szukaj karty z remaining_cards o takim samym kolorze
    for (size_t i = 0; i < remaining_cards.size(); i++) {
        if (remaining_cards[i].getColor() == first_card_color) {
            Card chosen_card = remaining_cards[i];
            remaining_cards.erase(remaining_cards.begin() + i); // Usuń wybraną kartę
            return chosen_card;
        }
    }

    // Jeśli nie znaleziono karty o takim samym kolorze, wybierz losową kartę
    int random_index = 0;
    Card chosen_card = remaining_cards[random_index];
    remaining_cards.erase(remaining_cards.begin() + random_index); // Usuń wybraną kartę
    return chosen_card;
}


struct sockaddr_in get_server_address(ClientParams params) {
  char const *host = params.host.c_str();
  uint16_t port = params.port;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));

  if(params.ipv4) hints.ai_family = AF_INET; // IPv4
  else if(params.ipv6) hints.ai_family = AF_INET6;
  else hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo *address_result;
  int errcode = getaddrinfo(host, NULL, &hints, &address_result);
  if (errcode != 0) {
    fatal("getaddrinfo: %s", gai_strerror(errcode));
  }

  struct sockaddr_in send_address;
  send_address.sin_family = AF_INET; // IPv4
  send_address.sin_addr.s_addr =     // IP address
      ((struct sockaddr_in *)(address_result->ai_addr))->sin_addr.s_addr;
  send_address.sin_port = htons(port); // port from the command line

  freeaddrinfo(address_result);

  return send_address;
}