#ifndef MIM_COMMON_H
#define MIM_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <unordered_map>
#include <vector>
#include <map>
#include <string>
#include <iostream>

#define CONNECTIONS 5
#define BUFFER_SIZE 100
#define CLIENTS 4
#define ROUNDS 13

class Card {
private:
    std::string gracz;
    std::string wartosc;
    std::string color;

public:
    Card() : gracz(""), wartosc("2"), color("") {}
    Card(const std::string& gracz, const std::string& wartosc, const std::string& color)
        : gracz(gracz), wartosc(wartosc), color(color) {}

    std::string getGracz() const { return gracz; }
    std::string getWartosc() const { return wartosc; }
    std::string getColor() const { return color; }

    void setGracz(const std::string& gracz) { this->gracz = gracz; }
    void setWartosc(const std::string& wartosc) { this->wartosc = wartosc; }
    void setColor(const std::string& color) { this->color = color; }

    int wartośćToValue() const {
        static std::unordered_map<std::string, int> valueMap = {
            {"2", 0}, {"3", 1}, {"4", 2}, {"5", 3}, {"6", 4},
            {"7", 5}, {"8", 6}, {"9", 7}, {"10", 8},
            {"J", 9}, {"Q", 10}, {"K", 11}, {"A", 12}
        };
        return valueMap.at(wartosc);
    }

    bool greaterEq(const Card& other) const {
        int thisValue = wartośćToValue();
        int otherValue = other.wartośćToValue();
        return thisValue >= otherValue;
    }
    void printCard() {
        std::cout << "Player: " << gracz << ", Value: " << wartosc << ", Color: " << color << std::endl;
    }
};

extern std::vector<Card> cards_in_round;

struct sockaddr_in get_server_address(char const *host, uint16_t port);
ssize_t	readn(int fd, void *vptr, size_t n);
ssize_t	writen(int fd, const void *vptr, size_t n);
void install_signal_handler(int signal, void (*handler)(int), int flags);
uint16_t read_port(char const *string);
int send_trick(int socket_fd, std::string card_list, int numer_lewy);
std::string read_trick(int socket_fd, std::string position, int expected_trick_number);
bool compareCards(const std::string& card1, const std::string& card2,
 const std::unordered_map<char, int>& cardValueMap);
 std::vector<std::string> parseAndSortCards(const std::string& cardString);
int set_nonblocking(int fd);
Card createCardFromString(const std::string& cardStr, const std::string& gracz);


class DealType {
public:
    virtual int countPoints(const std::vector<std::string>& cardsN, const std::vector<std::string>& cardsE,
                            const std::vector<std::string>& cardsS, const std::vector<std::string>& cardsW) const = 0;
    std::string id;
    virtual ~DealType() {} // Wirtualny destruktor
    std::string toString() const {
        return id; // Konwersja DealType na std::string
    }
};

// Struktura przechowująca informacje o rozdaniu
struct Deal {
    DealType* dealType;
    std::string startingClient;
    std::map<std::string, std::vector<std::string>> cards;

    Deal() {
        cards["N"] = std::vector<std::string>();
        cards["E"] = std::vector<std::string>();
        cards["S"] = std::vector<std::string>();
        cards["W"] = std::vector<std::string>();
    }
};

#endif
