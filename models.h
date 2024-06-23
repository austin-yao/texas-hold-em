#include <stdio.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>

struct Player
{
    std::string username;
    std::string connection;
    int fd;
    int stack;
    bool active;
    bool sb;
    bool bb;
    bool utg;
    bool inHand;
    int amountInStreet;
    bool allIn;
    std::vector<std::string> cards;
};

struct Hand
{
    std::vector<Player *> playersInHand;
    int playersRemaining;
    int pot;
    int toCall;
    int prevRaise;
    int lastToBet;
    int sb;
    int bb;
};

struct Game
{
    std::vector<Player> seats;
    int numPlayers;
    bool started;
};

std::vector<std::string>
    cards = {
        "2h", "3h", "4h", "5h", "6h",
        "7h", "8h", "9h", "Th", "Jh",
        "Qh", "Kh", "Ah", "2s", "3s",
        "4s", "5s", "6s", "7s", "8s",
        "9s", "Ts", "Js", "Qs", "Ks",
        "As", "2c", "3c", "4c", "5c",
        "6c", "7c", "8c", "9c", "Tc",
        "Jc", "Qc", "Kc", "Ac", "2d",
        "3d", "4d", "5d", "6d", "7d",
        "8d", "9d", "Td", "Jd", "Qd",
        "Kd", "Ad"};

std::unordered_map<char, int> cardToNum = {
    {'2', 0},
    {'3', 1},
    {'4', 2},
    {'5', 3},
    {'6', 4},
    {'7', 5},
    {'8', 6},
    {'9', 7},
    {'T', 8},
    {'J', 9},
    {'Q', 10},
    {'K', 11},
    {'A', 12}};

bool compare(Player p1, Player p2);