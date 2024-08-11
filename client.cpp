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

    // Use parsed parameters
    std::cout << "Host: " << params.host << std::endl;
    std::cout << "Port: " << params.port << std::endl;
    std::cout << "IPv4: " << (params.ipv4 ? "true" : "false") << std::endl;
    std::cout << "IPv6: " << (params.ipv6 ? "true" : "false") << std::endl;
    std::cout << "Place: " << params.place << std::endl;
    std::cout << "Automatic: " << (params.automatic ? "true" : "false") << std::endl;

  
    const char *host= params.host.c_str();
    // const char *host = argv[1];
    // uint16_t port = read_port(argv[2]);
    struct sockaddr_in server_address = get_server_address(host, params.port);

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    if (connect(socket_fd, (struct sockaddr *) &server_address,
                (socklen_t) sizeof(server_address)) < 0) {
        syserr("cannot connect to the server");
    }

    printf("connected to %s:%" PRIu16 "\n", host, params.port);

    
    send_IAM(socket_fd,params.place);
    std::vector<std::string> sortedCards = read_deal(socket_fd);
    std:: cout << "CARDS\n";
    for (int i = 0; i < 13; i++) {
        std::cout << sortedCards[i] << " ";
    }
    std:: cout << '\n';

    for(int i = 1; i <= ROUNDS; i++) {
        read_trick(socket_fd, "CLIENT" ,i);
        send_trick(socket_fd,sortedCards[i-1],i);
    }
    


    close(socket_fd);

    
    return 0;
}
