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

    return params;
}

void send_IAM(int socket_fd, char place) {
    static char line[BUFFER_SIZE];
    memset(line, 0, BUFFER_SIZE);
    snprintf(line, BUFFER_SIZE, "IAM%c\r\n", place);

    // Wysyłanie wiadomości
    size_t data_to_send = strnlen(line, BUFFER_SIZE);
    ssize_t written_length = writen(socket_fd, line, data_to_send);
    if (written_length < 0) {
        syserr("writen");
    } else if ((size_t) written_length != data_to_send) {
        fatal("incomplete writen");
    }
}

std::vector<std::string> read_deal(int socket_fd, char *ip_sender,
 uint16_t port_sender, char *ip_local, uint16_t port_local) {
    char buffer[BUFFER_SIZE];
    ssize_t received_bytes = read(socket_fd, buffer, BUFFER_SIZE);
    std::string card_list;
    if (received_bytes < 0) {
        error("error when reading message from connection");
        close(socket_fd);
    } else if (received_bytes == 0) {
        syserr("ending connection\n");
        close(socket_fd);
    } else {
        print_formatted_message(buffer, received_bytes, ip_sender, port_sender, ip_local, port_local);

        buffer[received_bytes] = '\0';  // Dodaj znak końca ciągu
        
        // Podział stringa na poszczególne części
        std::string message(buffer);
        std::string deal_type;
        

        // Sprawdź, czy wiadomość rozpoczyna się od "DEAL"
        if (message.substr(0, 4) == "DEAL") {
            deal_type = message.substr(4, 1); // Typ rozdania to jednoliterowy string
            card_list = message.substr(6, message.length() - 8); // Reszta to lista kart, -3 aby usunąć \r\n
        } else {
            syserr("Invalid message format. Expected message to start with 'DEAL'.");
        }
    }
    std::vector<std::string> sortedCards = parseAndSortCards(card_list);
    return sortedCards;
}

void read_score(int socket_fd, char *ip_sender, uint16_t port_sender, char *ip_local, uint16_t port_local) {
    char buffer[BUFFER_SIZE];
    ssize_t received_bytes = read(socket_fd, buffer, BUFFER_SIZE);

    if (received_bytes <= 0) {
        error("error when reading message from connection");
        close(socket_fd);
    } else {
        

        print_formatted_message(buffer, received_bytes, ip_sender, port_sender, ip_local, port_local);
    }
}