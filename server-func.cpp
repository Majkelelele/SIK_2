#include "server-func.h"
#include "common.h"
#include "err.h"
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
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>


#define TIMEOUT -1

pthread_mutex_t global_mutex;

int main_descriptor;

pthread_mutex_t mutexes[CLIENTS];
pthread_barrier_t final_barrier;
pthread_barrier_t clients_barrier;
std::vector<Deal> deals;
bool finish = false;
std::string current_cards_list = "";
int current_deal;
int players_played_in_round;
std::string who_won;


std::map<std::string, int> position_id_map;
std::map<std::string, int> total_points_round_map;
std::map<std::string, int> total_points_overall_map;

void initilizeMaps() {
  position_id_map["N"] = -1;
  position_id_map["E"] = -1;
  position_id_map["S"] = -1;
  position_id_map["W"] = -1;
  total_points_round_map["N"] = 0;
  total_points_round_map["E"] = 0;
  total_points_round_map["S"] = 0;
  total_points_round_map["W"] = 0;
  total_points_overall_map["N"] = 0;
  total_points_overall_map["E"] = 0;
  total_points_overall_map["S"] = 0;
  total_points_overall_map["W"] = 0;
}

// Funkcja do rozdzielenia linii na poszczególne karty
std::vector<std::string> splitCards(const std::string &line) {
  std::vector<std::string> cards;
  std::stringstream ss(line);
  std::string card;
  while (ss >> card) {
    cards.push_back(card);
  }
  return cards;
}

// Funkcja do wczytywania pliku i parsowania rozdań
void parseDealsFromFile(const std::string &filename) {
  std::ifstream file(filename);

  if (!file.is_open())
    error("Nie można otworzyć pliku: ");

  std::string line;
  while (std::getline(file, line)) {
    Deal deal;
    int deal_type =
        line.front() - '0'; // Pierwszy znak linii reprezentuje typ rozdania
    deal.startingClient = line.back();

    // Ustawienie odpowiedniego typu rozdania
    switch (deal_type) {
    case 1:
      deal.dealType = new NoTricks();
      break;
    case 2:
      deal.dealType = new NoHearts();
      break;
    case 3:
      deal.dealType = new NoQueens();
      break;
    case 4:
      deal.dealType = new NoJacksKings();
      break;
    case 5:
      deal.dealType = new NoKingOfHearts();
      break;
    case 6:
      deal.dealType = new NoSeventhAndLastTrick();
      break;
    case 7:
      deal.dealType = new Rogue();
      break;
    default:
      std::cerr << "Nieznany typ rozdania: " << deal_type << std::endl;
      continue;
    }

    std::getline(file, line);
    deal.cards.at("N") = splitCards(line);

    std::getline(file, line);
    deal.cards.at("E") = splitCards(line);

    std::getline(file, line);
    deal.cards.at("S") = splitCards(line);

    std::getline(file, line);
    deal.cards.at("W") = splitCards(line);

    deals.push_back(deal);
  }

  file.close();
}

// Funkcja do wyświetlania informacji o rozdaniach
void printDeals(const std::vector<Deal> &deals) {
  for (const auto &deal : deals) {
    std::cout << "Typ rozdania: " << deal.dealType->toString() << "\n";
    std::cout << "Pierwszy wychodzi klient na miejscu: " << deal.startingClient
              << "\n";
    std::cout << "Karty klienta N: ";
    for (const auto &card : deal.cards.at("N")) {
      std::cout << card << " ";
    }
    std::cout << "\nKarty klienta E: ";
    for (const auto &card : deal.cards.at("E")) {
      std::cout << card << " ";
    }
    std::cout << "\nKarty klienta S: ";
    for (const auto &card : deal.cards.at("S")) {
      std::cout << card << " ";
    }
    std::cout << "\nKarty klienta W: ";
    for (const auto &card : deal.cards.at("W")) {
      std::cout << card << " ";
    }
    std::cout << "-------------------------\n";
  }
}

void printUsage() {
  error("Usage: server [-p <port>] -f <file> [-t <timeout>]\n");
}

void parseArguments(int argc, char *argv[], int *port, std::string *file,
                    int *timeout) {
  // Default values
  *port = 0;
  *timeout = 5;

  std::map<std::string, std::string> args;
  std::vector<std::string> params(argv + 1, argv + argc);

  // Parse arguments
  for (size_t i = 0; i < params.size(); ++i) {
    if (params[i] == "-p" && i + 1 < params.size()) {
      if (args.find("-p") == args.end()) { // Use the first occurrence
        *port = read_port(params[i + 1].c_str());
      }
      ++i;
    } else if (params[i] == "-f" && i + 1 < params.size()) {
      if (args.find("-f") == args.end()) { // Use the first occurrence
        args["-f"] = params[i + 1];
      }
      ++i;
    } else if (params[i] == "-t" && i + 1 < params.size()) {
      if (args.find("-t") == args.end()) { // Use the first occurrence
        args["-t"] = params[i + 1];
      }
      ++i;
    } else {
      printUsage();
      exit(EXIT_FAILURE);
    }
  }

  // Validate required arguments
  if (args.find("-f") == args.end()) {
    printUsage();
    error("The -f <file> parameter is mandatory.\n");
  }

  *file = args["-f"];

  if (args.find("-t") != args.end()) {
    *timeout = std::stoi(args["-t"]);
  }
  *timeout = *timeout * MILISECONDS_IN_SECOND;
}

void send_deal_to_client(int client_fd, const std::string &deal_type,
                         std::string starting_client,
                         const std::vector<std::string> &cards) {
  char line[BUFFER_SIZE];
  memset(line, 0, BUFFER_SIZE);

  // Formatowanie wiadomości
  std::string card_list;
  for (const auto &card : cards) {
    card_list += card;
  }

  snprintf(line, BUFFER_SIZE, "DEAL%s%s%s\r\n", deal_type.c_str(),
           starting_client.c_str(), card_list.c_str());

  size_t data_to_send = strnlen(line, BUFFER_SIZE);
  ssize_t written_length = writen(client_fd, line, data_to_send);
  if (written_length < 0) {
    syserr("writen");
  } else if ((size_t)written_length != data_to_send) {
    fatal("incomplete writen");
  }
}

std::string find_who_next(std::string current) {
  if (current == "N")
    return "E";
  if (current == "E")
    return "S";
  if (current == "S")
    return "W";
  else
    return "N";
}

std::string process_IAM_message(int client_fd, const std::string &ip_sender,
 uint16_t port_sender, const std::string &ip_local, uint16_t port_local) {
  char buffer[BUFFER_SIZE];
  ssize_t received_bytes = readn(client_fd, buffer, BUFFER_SIZE);

  if (received_bytes < 0) {
    close(client_fd);
    error("Error when reading message from connection", client_fd);
  } else if (received_bytes == 0) {
    close(client_fd);
    client_fd = -1; 
    return "";
  } else {
    std::string message_buffer(buffer, received_bytes);

    const std::string prefix = "IAM";
    const std::string suffix = "\r\n";

    size_t start_pos = 0;
    while ((start_pos = message_buffer.find(prefix, start_pos)) != std::string::npos) {
        size_t end_pos = message_buffer.find(suffix, start_pos + prefix.size());

        if (end_pos != std::string::npos) {
            std::string message = message_buffer.substr(start_pos, end_pos + suffix.size() - start_pos);
            std::string place = message.substr(prefix.size(), message.size() - prefix.size() - suffix.size());
            print_formatted_message(buffer, received_bytes, ip_sender, port_sender, ip_local, port_local);
            return place;
        } 
    }
  }
  return "";
} 


void assign_position_to_index(std::string pos, int id) {
  if (pthread_mutex_lock(&global_mutex) != 0) {
    syserr("Error locking mutex %d\n");
  }
  if (position_id_map.at(pos) != -1)
    std::cout << "position already taken \n";
  else
    position_id_map.at(pos) = id;

  if (pthread_mutex_unlock(&global_mutex) != 0) {
    syserr("Error unlocking mutex %d\n");
  }
}

int accept_client(int client_id, struct sockaddr_in *client_address,
                  socklen_t *client_address_len) {

  if (pthread_mutex_lock(&mutexes[client_id]) != 0) {
    syserr("Error locking mutex %d\n");
  }



  int client_fd = accept(main_descriptor, (struct sockaddr *)client_address,
                         client_address_len);
  if (client_fd < 0)
    syserr("accept");


  if (client_id < CLIENTS - 1 &&
      pthread_mutex_unlock(&mutexes[(client_id + 1) % CLIENTS]) != 0) {
    syserr("Error unlocking mutex %d\n");
  }
  return client_fd;
}

std::string find_who_won() {
  if (cards_in_round.empty()) {
    return "No cards in round";
  }

  const Card &leadingCard = cards_in_round.front();
  std::string leadingColor = leadingCard.getColor();

  const Card *winningCard = &leadingCard;

  for (const Card &card : cards_in_round) {
    if (card.getColor() == leadingColor && card.greaterEq(*winningCard)) {
      winningCard = &card;
    }
  }

  return winningCard->getGracz();
};

std::string summarize_trick(int current_round, int trick_number) {
  int points_to_add = deals[current_round].dealType->countPoints(cards_in_round, trick_number);
  std::string who_won = find_who_won();
  total_points_round_map.at(who_won) += points_to_add;
  total_points_overall_map.at(who_won) += points_to_add;
  return who_won;
}



void trick_communication(int client_id, std::string position, int client_fd, const std::string &ip_sender,
 uint16_t port_sender, const std::string &ip_local, uint16_t port_local, int current_round) {

  pthread_mutex_trylock(&mutexes[position_id_map.at(position)]);
  pthread_barrier_wait(&clients_barrier);
  if (client_id == CLIENTS - 1) {
    if (pthread_mutex_unlock(
            &mutexes[position_id_map.at(deals[current_round].startingClient)]) != 0) {
      error("Error unlocking mutex %d\n");
    }
  }

  for (int i = 1; i <= ROUNDS; i++) {
    if (pthread_mutex_lock(&mutexes[client_id]) != 0) {
      error("Error locking mutex %d\n");
    }

    if (pthread_mutex_lock(&global_mutex) != 0) {
      error("Error locking mutex %d\n");
    }

    players_played_in_round++;
    if(players_played_in_round == 1) {
      cards_in_round.clear();
      current_cards_list = "";
    }
    send_trick(client_fd, current_cards_list, i);
    current_cards_list += read_trick(client_fd, position, i, ip_sender, port_sender, ip_local, port_local);
    std::string next_position = find_who_next(position);

    if (players_played_in_round == 4) {
      next_position = summarize_trick(current_round, i);
      who_won = next_position;
      players_played_in_round = 0;  
    }
    int next_to_play = position_id_map.at(next_position);

    if (pthread_mutex_unlock(&mutexes[next_to_play]) != 0) {
      syserr("Error unlocking mutex %d\n");
    }
    if (pthread_mutex_unlock(&global_mutex) != 0) {
      syserr("Error unlocking mutex %d\n");
    }

    pthread_barrier_wait(&clients_barrier);
    send_taken(client_fd,current_cards_list,i,who_won);
    pthread_barrier_wait(&clients_barrier);
  }
}

void send_score_to_client(int client_fd) {
    char line[BUFFER_SIZE];
    memset(line, 0, BUFFER_SIZE);

    std::string score_message = "SCORE";
    for (const auto& pair : total_points_round_map) {
      const std::string& position = pair.first;
      int score = pair.second;
      score_message += position + std::to_string(score);
    }
    score_message += "\r\n";
    snprintf(line, BUFFER_SIZE, "%s", score_message.c_str());
    size_t data_to_send = strnlen(line, BUFFER_SIZE);
    ssize_t written_length = writen(client_fd, line, data_to_send);
    if (written_length < 0) {
        error("writen");
    } else if ((size_t)written_length != data_to_send) {
        fatal("incomplete writen");
    }
}

void send_total_to_client(int client_fd) {
    char line[BUFFER_SIZE];
    memset(line, 0, BUFFER_SIZE);

    std::string total_message = "TOTAL";
    for (const auto& pair : total_points_overall_map) {
        const std::string& position = pair.first;
        int score = pair.second;
        total_message += position + std::to_string(score);
    }
    total_message += "\r\n";

    snprintf(line, BUFFER_SIZE, "%s", total_message.c_str());

    size_t data_to_send = strnlen(line, BUFFER_SIZE);
    ssize_t written_length = writen(client_fd, line, data_to_send);
    if (written_length < 0) {
        syserr("writen");
    } else if ((size_t)written_length != data_to_send) {
        fatal("incomplete writen");
    }
}


void clear_map() {
  total_points_round_map["N"] = 0;
  total_points_round_map["E"] = 0;
  total_points_round_map["S"] = 0;
  total_points_round_map["W"] = 0;
}

void *handle_connection(void *client_id_ptr) {
  struct sockaddr_in client_address;
  socklen_t client_address_len = sizeof(client_address);

  int client_id = *(int *)client_id_ptr;
  free(client_id_ptr);

  int client_fd =
      accept_client(client_id, &client_address, &client_address_len);
  if (client_fd < 0) {
      syserr("accept");
  }

  // Pobieranie adresu IP klienta (sender) i portu
  char ip_sender[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_address.sin_addr, ip_sender, sizeof(ip_sender));
  uint16_t port_sender = ntohs(client_address.sin_port);

  // Pobieranie lokalnego adresu IP (local) i portu
  struct sockaddr_in local_address;
  socklen_t local_address_len = sizeof(local_address);
  if (getsockname(client_fd, (struct sockaddr *)&local_address, &local_address_len) == -1) {
      syserr("getsockname");
  }

  char ip_local[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &local_address.sin_addr, ip_local, sizeof(ip_local));
  uint16_t port_local = ntohs(local_address.sin_port);

  // Przekształcanie na std::string, aby dopasować do typu funkcji
  std::string ip_sender_str(ip_sender);
  std::string ip_local_str(ip_local);

  std::string position = process_IAM_message(client_fd, ip_sender,port_sender,ip_local,port_local);

  if (position == "") syserr("error reading IAM");
  assign_position_to_index(position, client_id);

  pthread_barrier_wait(&clients_barrier);

  // DEAL
  for(size_t i = 0; i < deals.size(); i++) {
    Deal deal = deals[i];
    std::string deal_type = deal.dealType->id;
    send_deal_to_client(client_fd, deal_type, deal.startingClient,
                        deal.cards.at(position));
    

    pthread_barrier_wait(&clients_barrier);
    trick_communication(client_id, position, client_fd, ip_sender,port_sender,ip_local,port_local,i);
    pthread_barrier_wait(&clients_barrier);
    send_score_to_client(client_fd);
    pthread_barrier_wait(&clients_barrier);
    clear_map();
    pthread_barrier_wait(&clients_barrier);
  }
  send_total_to_client(client_fd);
  
  

  pthread_barrier_wait(&final_barrier);

  return 0;
}

int start_server(int socket_fd) {
  current_deal = 0;
  pthread_t threads[CLIENTS];
  players_played_in_round = 0;
  who_won = "";

  if (pthread_barrier_init(&final_barrier, NULL, CLIENTS + 1) != 0)
    syserr("final barrier initialization failed");
  if (pthread_barrier_init(&clients_barrier, NULL, CLIENTS) != 0)
    syserr("clients barrier initialization failed");

  if (pthread_mutex_init(&global_mutex, NULL) != 0) {
    error("Failed to initialize mutex\n");
    return -1;
  }

  for (int i = 0; i < CLIENTS; i++) {
    if (pthread_mutex_init(&mutexes[i], NULL) != 0) {
      error("Failed to initialize mutex %d\n", i);
      return 1;
    }
  }

  // Zablokowanie wszystkich muteksów poza tym który będzie odbierać pierwszego klienta
  for (int i = 1; i < CLIENTS; i++) {
    if (pthread_mutex_lock(&mutexes[i]) != 0) {
      error("Error locking mutex %d\n", i);
      return 1;
    }
  }

  // Inicjalizacja głównego deskryptora
  main_descriptor = socket_fd;

  initilizeMaps();

  // Tworzenie wątków
  for (int i = 0; i < CLIENTS; i++) {
    int *thread_id = (int *)malloc(sizeof(int));
    *thread_id = i;
    if (pthread_create(&threads[i], NULL, handle_connection, thread_id) != 0) {
      error("Error creating thread %d\n", i);
      return -1;
    }

    if (pthread_detach(threads[i]) != 0) {
      error("Error detaching thread %d\n", i);
      return -1;
    }
  }
  return 1;
}

void destroy_and_finish() {
  pthread_mutex_destroy(&global_mutex);

  for (int i = 0; i < CLIENTS; i++) {
    if (pthread_mutex_destroy(&mutexes[i]) != 0) {
      syserr("Failed to destroy mutex %d\n");
    }
  }

  if (pthread_barrier_destroy(&final_barrier) != 0)
    syserr("final barrier destruction failed");
  if (pthread_barrier_destroy(&clients_barrier) != 0)
    syserr("clients barrier destruction failed");
}

int send_taken(int socket_fd, std::string card_list, int numer_lewy, std::string client_position) {
  char line[BUFFER_SIZE];
  memset(line, 0, BUFFER_SIZE);

  // Formatowanie wiadomości TAKEN
  snprintf(line, BUFFER_SIZE, "TAKEN%s%s%s\r\n",
           (std::to_string(numer_lewy)).c_str(), 
           card_list.c_str(), 
           client_position.c_str());

  size_t data_to_send = strnlen(line, BUFFER_SIZE);
  ssize_t written_length = writen(socket_fd, line, data_to_send);
  
  if (written_length < 0) {
    syserr("writen");
  } else if ((size_t)written_length != data_to_send) {
    fatal("incomplete writen");
  }
  
  return written_length;
}
