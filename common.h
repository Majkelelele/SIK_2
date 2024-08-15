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
#include <algorithm>

#define CONNECTIONS 5
#define BUFFER_SIZE 100
#define CLIENTS 4
#define ROUNDS 13
#define MILISECONDS_IN_SECOND 1000
#define SEVEN 7
#define LAST 13

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
ssize_t readn(int fd, char *buf, size_t buf_size);
ssize_t	writen(int fd, const void *vptr, size_t n);
void install_signal_handler(int signal, void (*handler)(int), int flags);
uint16_t read_port(char const *string);
int send_trick(int socket_fd, std::string card_list, int numer_lewy);
std::string read_trick(int socket_fd, std::string position, int expected_trick_number,
 const std::string &ip_sender, uint16_t port_sender, const std::string &ip_local, uint16_t port_local);
std::vector<std::string> parseCards(const std::string& cardString);
int set_nonblocking(int fd);
Card createCardFromString(const std::string& cardStr, const std::string& gracz);
void print_formatted_message(char *buffer, ssize_t received_bytes, const std::string &ip_sender,
 uint16_t port_sender, const std::string &ip_local, uint16_t port_local);



class DealType {
public:
    virtual int countPoints(const std::vector<Card>& trick, int trick_number) const = 0;
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




// Nie brać lew, 1 punkt za każdą wziętą lewę
class NoTricks : public DealType {
public:
    NoTricks() { id = "1"; }

    int countPoints(const std::vector<Card>& /*trick*/, int /*trick_number*/) const override {
        return 1; 
    }
};

// Nie brać kierów, 1 punkt za każdego wziętego kiera w lewie
class NoHearts : public DealType {
public:
    NoHearts() { id = "2"; }

    int countPoints(const std::vector<Card>& trick, int /*trick_number*/) const override {
        return std::count_if(trick.begin(), trick.end(), [](const Card& c) { return c.getColor() == "H"; });
    }
};

// Nie brać dam, 5 punktów za każdą damę w lewie
class NoQueens : public DealType {
public:
    NoQueens() { id = "3"; }

    int countPoints(const std::vector<Card>& trick, int /*trick_number*/) const override {
        return std::count_if(trick.begin(), trick.end(), [](const Card& c) { return c.getWartosc() == "Q"; }) * 5;
    }
};

// Nie brać panów (waletów i króli), 2 punkty za każdego pana w lewie
class NoJacksKings : public DealType {
public:
    NoJacksKings() { id = "4"; }

    int countPoints(const std::vector<Card>& trick, int /*trick_number*/) const override {
        return std::count_if(trick.begin(), trick.end(), [](const Card& c) {
            return c.getWartosc() == "J" || c.getWartosc() == "K";
        }) * 2;
    }
};

// Nie brać króla kier, 18 punktów za jego wzięcie
class NoKingOfHearts : public DealType {
public:
    NoKingOfHearts() { id = "5"; }

    int countPoints(const std::vector<Card>& trick, int /*trick_number*/) const override {
        return std::count_if(trick.begin(), trick.end(), [](const Card& c) {
            return c.getWartosc() == "K" && c.getColor() == "H";
        }) * 18;
    }
};

// Nie brać siódmej i ostatniej lewy, 10 punktów za każdą z tych lew
class NoSeventhAndLastTrick : public DealType {
public:
    NoSeventhAndLastTrick() { id = "6"; }

    int countPoints(const std::vector<Card>& /*trick*/, int trick_number) const override {
        return (trick_number == SEVEN || trick_number == LAST) * 10;
    }
};

// Rozbójnik: Punkty za wszystko powyższe
class Rogue : public DealType {
public:
    Rogue() { id = "7"; }

    int countPoints(const std::vector<Card>& trick, int trick_number) const override {

        return NoTricks().countPoints(trick,trick_number) + 
               NoHearts().countPoints(trick, trick_number) +
               NoQueens().countPoints(trick, trick_number) +
               NoJacksKings().countPoints(trick, trick_number) +
               NoKingOfHearts().countPoints(trick, trick_number) +
               NoSeventhAndLastTrick().countPoints(trick, trick_number);
    }
};

#endif
