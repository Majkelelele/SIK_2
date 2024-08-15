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

static const char exit_string[] = "exit";

int main(int argc, char *argv[]) {
    if (argc < 6) {
        printUsage();
        return 1;
    }

    ClientParams params = parseArgumentsClient(argc, argv);
    struct sockaddr_in server_address = get_server_address(params.host.c_str(), params.port);    
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    if (connect(socket_fd, (struct sockaddr *) &server_address,
                (socklen_t) sizeof(server_address)) < 0) {
        syserr("cannot connect to the server");
    }

    char ip_server[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &server_address.sin_addr, ip_server, sizeof(ip_server));
    uint16_t port_server = ntohs(server_address.sin_port);



    struct sockaddr_in local_address;
    socklen_t address_length = sizeof(local_address);

    // Pobranie lokalnego adresu IP i portu
    if (getsockname(socket_fd, (struct sockaddr *)&local_address, &address_length) == -1) {
        perror("getsockname failed");
        close(socket_fd);
        return 1;
    }

    char ip_local[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_address.sin_addr, ip_local, sizeof(ip_local));

    uint16_t local_port = ntohs(local_address.sin_port);
    
    send_IAM(socket_fd,params.place,ip_server, port_server, ip_local, local_port);
    bool disconnected = false;
    std::string result = "";
    while(!disconnected) {
        std::vector<Card> remaining_cards = read_deal(socket_fd, ip_server, port_server, ip_local, local_port);
        if(remaining_cards.size() == 0) disconnected = true;
        for(int i = 1; !disconnected && i <= ROUNDS; i++) {
            std::string card_list = read_trick(socket_fd, "CLIENT", i, ip_server, port_server, ip_local, local_port);
            Card choosen_card = choose_card(card_list, remaining_cards);
            std::cout << "remaining cards size = " << remaining_cards.size() << "\n";
            
            send_trick(socket_fd,choosen_card.toString(),i, ip_server, port_server, ip_local, local_port);
            read_taken(socket_fd, ip_server, port_server, ip_local, local_port);
        }
        if(!disconnected) read_score(socket_fd, ip_server, port_server, ip_local, local_port);
    }
    
    

    close(socket_fd);
    

    
    return 0;
}
