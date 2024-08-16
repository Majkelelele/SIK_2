#include "common.h"
#include "err.h"
#include "server-func.h"
#include <algorithm>
#include <arpa/inet.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <inttypes.h>
#include <iostream>
#include <limits.h>
#include <map>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <iomanip>


std::vector<Card> cards_in_round;




ssize_t readn(int fd, char *buf, size_t buf_size) {
  ssize_t nread;
  size_t totalRead = 0;
  char c;
  const std::string delimiter = "\r\n";
  size_t delimiter_len = delimiter.length();
  size_t delimiter_pos = 0;
  while (totalRead < buf_size) {
    nread = read(fd, &c, 1); 
    if (nread < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return totalRead;
      } else {
        return -1;
      }
    } else if (nread == 0) {
      break;
    }
    buf[totalRead++] = c; 

    if (c == delimiter[delimiter_pos]) {
      delimiter_pos++;
      if (delimiter_pos == delimiter_len) {
        return totalRead;
      }
    } else {
      delimiter_pos = 0;
    }
  }
  return totalRead;
}


ssize_t writen(int fd, const void *vptr, size_t n) {
  ssize_t nleft, nwritten;
  const char *ptr;

  ptr = static_cast<const char *>(vptr);
  nleft = n;
  while (nleft > 0) {
    if ((nwritten = write(fd, ptr, nleft)) <= 0)
      return nwritten; // error

    nleft -= nwritten;
    ptr += nwritten;
  }
  return n;
}


uint16_t read_port(char const *string) {
  char *endptr;
  unsigned long port = strtoul(string, &endptr, 10);
  if ((port == ULONG_MAX && errno == ERANGE) || *endptr != 0 || port == 0 ||
      port > UINT16_MAX) {
    fatal("%s is not a valid port number", string);
  }
  return (uint16_t)port;
}

int send_trick(int socket_fd, std::string card_list, int numer_lewy,
const std::string &ip_sender, uint16_t port_sender, const std::string &ip_local, uint16_t port_local) {
  char line[BUFFER_SIZE];
  memset(line, 0, BUFFER_SIZE);

  snprintf(line, BUFFER_SIZE, "TRICK%s%s\r\n",
           (std::to_string(numer_lewy)).c_str(), card_list.c_str());

  size_t data_to_send = strnlen(line, BUFFER_SIZE);
  print_formatted_message(line, data_to_send, ip_local, port_local, ip_sender, port_sender);
  ssize_t written_length = writen(socket_fd, line, data_to_send);
  if (written_length < 0) {
    syserr("writen");
  } else if ((size_t)written_length != data_to_send) {
    fatal("incomplete writen");
  }
  
  return written_length;
}



Card createCardFromString(const std::string &cardStr,
                          const std::string &gracz) {
  std::string value;
  std::string color;

  // Determine if cardStr has a 2-character value (like "10") or 1-character
  if (cardStr.length() == 3) {
      value = cardStr.substr(0, 2);
      color = cardStr.substr(2, 1);  
  } else {
      value = cardStr.substr(0, 1);  
      color = cardStr.substr(1, 1);  
  }
  return Card(gracz, value, color);
}

std::string read_trick(int socket_fd, std::string position, int expected_trick_number,
 const std::string &ip_sender, uint16_t port_sender, const std::string &ip_local, uint16_t port_local) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    ssize_t received_bytes = readn(socket_fd, buffer, BUFFER_SIZE);

    std::string card;

    if (received_bytes < 0) {
        close(socket_fd);
        error("error when reading message from connection");
    } else if (received_bytes == 0) {
        close(socket_fd);
        return "disconnected";
    } else {
        print_formatted_message(buffer,received_bytes,ip_sender,port_sender,ip_local,port_local);
        buffer[received_bytes] = '\0'; 
        std::string message(buffer);
        std::ostringstream oss;
        oss << expected_trick_number;
        std::string expected_trick_number_str = oss.str();

        if (message.substr(0, 5) == "TRICK") {
            std::string trick_number_str = message.substr(5, expected_trick_number_str.length());

            if (trick_number_str != expected_trick_number_str) {
                std::cerr << "Unexpected trick number. Expected: " << expected_trick_number_str 
                          << ", but got: " << trick_number_str << std::endl;
                return card;
            }
            int next_to_read = 5 + expected_trick_number_str.length();
            card = message.substr(next_to_read, message.length() - next_to_read - 2);
        } else if(message.substr(0, 5) == "TOTAL") {
          return "disconnected";
        } 
        else {
            std::cerr << "Invalid message format. Expected message to start with 'TRICK'." 
                      << std::endl;
        }
    }

    if(position != "CLIENT") cards_in_round.push_back(createCardFromString(card, position));

    return card;
}

std::unordered_map<std::string, int> createCardValueMap() {
  return {{"2", 2},  {"3", 3},  {"4", 4}, {"5", 5},   {"6", 6},
          {"7", 7},  {"8", 8},  {"9", 9}, {"10", 10}, {"J", 11},
          {"Q", 12}, {"K", 13}, {"A", 14}};
}




std::vector<std::string> parseCards(const std::string &cardString) {
  std::unordered_map<std::string, int> cardValueMap = createCardValueMap();
  std::vector<std::string> cards;

  for (size_t i = 0; i < cardString.size(); i += 2) {
    if (i + 1 < cardString.size() && cardString[i] == '1' &&
        cardString[i + 1] == '0') {
      cards.push_back(
          cardString.substr(i, 3)); // "10" jest reprezentowane przez dwa znaki
      i += 1;                       // Przeskocz do następnej karty
    } else {
      cards.push_back(cardString.substr(i, 2));
    }
  }
  return cards;
}

int set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    syserr("fcntl F_GETFL");
    return -1;
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    syserr("fcntl F_SETFL");
    return -1;
  }

  return 0;
}

void print_formatted_message(char *buffer, ssize_t received_bytes, const std::string &ip_sender, uint16_t port_sender, const std::string &ip_local, uint16_t port_local) {
    // Pobranie bieżącego czasu
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // Formatowanie czasu jako YYYY-MM-DDTHH:MM:SS.sss
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&now_time), "%Y-%m-%dT%H:%M:%S");
    timestamp << '.' << std::setw(3) << std::setfill('0') << now_ms.count();

    std::stringstream output_message;
    output_message << "[" << ip_sender << ":" << port_sender << ","
                   << ip_local << ":" << port_local << ","
                   << timestamp.str() << "] ";

    std::string header_str = output_message.str();

    // Łączenie nagłówka i treści wiadomości
    std::string combined_message = header_str + std::string(buffer, received_bytes);

    // Wypisanie całej wiadomości na stdout za pomocą write
    ssize_t bytes_written = write(STDOUT_FILENO, combined_message.c_str(), combined_message.size());

    if (bytes_written < 0) {
        error("Error writing combined message to stdout");
    } else if (static_cast<size_t>(bytes_written) != combined_message.size()) {
        syserr("Incomplete combined message written to stdout");
    }

    // Natychmiastowe spłukanie buforów, aby upewnić się, że dane zostały wysłane
    if (fflush(stdout) != 0) {
        error("fflush");
    }
}



