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

std::map<std::string, int> position_id_map;
std::map<std::string, int> total_points_map;

void initilizeMaps() {
  position_id_map["N"] = -1;
  position_id_map["E"] = -1;
  position_id_map["S"] = -1;
  position_id_map["W"] = -1;
  total_points_map["N"] = 0;
  total_points_map["E"] = 0;
  total_points_map["S"] = 0;
  total_points_map["W"] = 0;
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
    std::cerr << "Nie można otworzyć pliku: " << filename << std::endl;

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

    // Pomijanie ewentualnej pustej linii pomiędzy rozdaniami
    std::getline(file, line);
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
  std::cout << "Usage: server [-p <port>] -f <file> [-t <timeout>]\n";
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
    std::cerr << "Error: The -f <file> parameter is mandatory.\n";
    printUsage();
    exit(EXIT_FAILURE);
  }

  // // Extract arguments
  // if (args.find("-p") != args.end()) {
  //     *port = args["-p"];
  // }

  *file = args["-f"];

  if (args.find("-t") != args.end()) {
    *timeout = std::stoi(args["-t"]);
  }
  *timeout = *timeout * 1000;
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

  // Wysyłanie wiadomości
  std::cout << "sending:" << line;
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

std::string process_IAM_message(int client_fd) {
  static std::map<int, std::string>
      client_buffers; // To store buffer per client
  char buffer[BUFFER_SIZE];
  ssize_t received_bytes = read(client_fd, buffer, BUFFER_SIZE);

  if (received_bytes < 0) {
    // Error occurred while reading
    error("Error when reading message from connection", client_fd);
    close(client_fd);
    client_fd = -1; // Mark the fd as closed
    return "";
  } else if (received_bytes == 0) {
    // Connection closed by the client
    std::cout << "Ending connection (" << client_fd << ")\n";
    close(client_fd);
    client_fd = -1; // Mark the fd as closed
    return "";
  } else {
    // Accumulate data in the local buffer for the client
    std::string &client_buffer = client_buffers[client_fd];
    client_buffer.append(buffer, received_bytes);

    // Define message boundaries
    const std::string prefix = "IAM";
    const std::string suffix = "\r\n";

    size_t start_pos = 0;
    while ((start_pos = client_buffer.find(prefix, start_pos)) !=
           std::string::npos) {
      size_t end_pos = client_buffer.find(suffix, start_pos + prefix.size());

      if (end_pos != std::string::npos) {
        // Extract the complete message
        std::string message = client_buffer.substr(
            start_pos, end_pos + suffix.size() - start_pos);

        // Remove processed message from buffer
        client_buffer.erase(0, end_pos + suffix.size());

        // Process the message
        std::string place = message.substr(
            prefix.size(), message.size() - prefix.size() - suffix.size());
        std::cout << "Miejsce przy stole: " << place << std::endl;

        // Return the place extracted from the message
        return place;
      } else {
        // No complete message found yet, break the loop and wait for more data
        break;
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

  printf("[%lu] thread is starting\n", (unsigned long)pthread_self());

  std::cout << "client id: " << client_id << '\n';

  int client_fd = accept(main_descriptor, (struct sockaddr *)client_address,
                         client_address_len);
  if (client_fd < 0)
    syserr("accept");
  std::cout << "accepted client"
            << "\n";

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

std::string summarize_trick() {

  int points_to_add = deals[0].dealType->countPoints(cards_in_round);
  std::string who_won = find_who_won();
  std:: cout << "Player" + who_won + "lost" + std::to_string(points_to_add) + "points" + '\n';
  total_points_map.at(who_won) += points_to_add;
  return who_won;
}

void trick_communication(int client_id, std::string position, int client_fd) {
  if (client_id == CLIENTS - 1) {
    if (pthread_mutex_unlock(
            &mutexes[position_id_map.at(deals[0].startingClient)]) != 0) {
      syserr("Error unlocking mutex %d\n");
    }
  }
  for (int i = 1; i <= ROUNDS; i++) {

    if (pthread_mutex_lock(&mutexes[client_id]) != 0) {
      syserr("Error locking mutex %d\n");
    }
    if (pthread_mutex_lock(&global_mutex) != 0) {
      syserr("Error locking mutex %d\n");
    }

    send_trick(client_fd, current_cards_list, i);
    current_cards_list += read_trick(client_fd, position, i);
    std::string next_position = find_who_next(position);

    players_played_in_round++;

    if (players_played_in_round == 4) {
      // countpoints
      std::cout << "position of 4th player" + position + '\n';
      std::cout << "CARDS LIST after round: " + current_cards_list + '\n';
      std::cout << "card in round: \n";
      for (size_t i = 0; i < cards_in_round.size(); i++) {
        cards_in_round[i].printCard();
      }
      next_position = summarize_trick();
      players_played_in_round = 0;
      cards_in_round.clear();
      current_cards_list = "";
    }

    int next_to_play = position_id_map.at(next_position);

    if (pthread_mutex_unlock(&global_mutex) != 0) {
      syserr("Error unlocking mutex %d\n");
    }
    if (pthread_mutex_unlock(&mutexes[next_to_play]) != 0) {
      syserr("Error unlocking mutex %d\n");
    }
  }
}

void send_score_to_client(int client_fd) {
    char line[BUFFER_SIZE];
    memset(line, 0, BUFFER_SIZE);

    // Tworzenie wiadomości SCORE
    std::string score_message = "SCORE";
    for (const auto& pair : total_points_map) {
      const std::string& position = pair.first;
      int score = pair.second;
      score_message += position + std::to_string(score);
    }
    score_message += "\r\n";

    // Przekopiowanie wiadomości do bufora
    snprintf(line, BUFFER_SIZE, "%s", score_message.c_str());

    // Wysyłanie wiadomości
    std::cout << "sending: " << line;
    size_t data_to_send = strnlen(line, BUFFER_SIZE);
    ssize_t written_length = writen(client_fd, line, data_to_send);
    if (written_length < 0) {
        syserr("writen");
    } else if ((size_t)written_length != data_to_send) {
        fatal("incomplete writen");
    }
}


void *handle_connection(void *client_id_ptr) {
  struct sockaddr_in client_address;
  socklen_t client_address_len = sizeof(client_address);

  int client_id = *(int *)client_id_ptr;
  free(client_id_ptr);

  int client_fd =
      accept_client(client_id, &client_address, &client_address_len);

  std::string position = process_IAM_message(client_fd);

  if (position == "")
    syserr("error reading IAM");
  assign_position_to_index(position, client_id);

  // after IAM
  pthread_barrier_wait(&clients_barrier);

  // DEAL
  Deal deal = deals[0];
  std::string deal_type = deal.dealType->id;
  send_deal_to_client(client_fd, deal_type, deal.startingClient,
                      deal.cards.at(position));

  pthread_barrier_wait(&clients_barrier);
  trick_communication(client_id, position, client_fd);
  pthread_barrier_wait(&clients_barrier);
  send_score_to_client(client_fd);
  

  pthread_barrier_wait(&final_barrier);
  std::cout << "total points received by " + position + " = " + std::to_string(total_points_map.at(position)) + "\n"; 

  return 0;
}

int prepare_shared_variables(int socket_fd) {
  current_deal = 0;
  pthread_t threads[CLIENTS];
  players_played_in_round = 0;

  if (pthread_barrier_init(&final_barrier, NULL, CLIENTS + 1) != 0)
    syserr("final barrier initialization failed");
  if (pthread_barrier_init(&clients_barrier, NULL, CLIENTS) != 0)
    syserr("clients barrier initialization failed");

  if (pthread_mutex_init(&global_mutex, NULL) != 0) {
    fprintf(stderr, "Failed to initialize mutex\n");
    return -1;
  }

  for (int i = 0; i < CLIENTS; i++) {
    if (pthread_mutex_init(&mutexes[i], NULL) != 0) {
      fprintf(stderr, "Failed to initialize mutex %d\n", i);
      return 1;
    }
  }

  // Zablokowanie wszystkich muteksów poza jednym
  for (int i = 1; i < CLIENTS; i++) {
    if (pthread_mutex_lock(&mutexes[i]) != 0) {
      fprintf(stderr, "Error locking mutex %d\n", i);
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
      fprintf(stderr, "Error creating thread %d\n", i);
      return -1;
    }

    if (pthread_detach(threads[i]) != 0) {
      fprintf(stderr, "Error detaching thread %d\n", i);
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