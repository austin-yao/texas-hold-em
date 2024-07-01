#ifndef MODELS_H
#define MODELS_H

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
    int amountInStreet;
    bool allIn;
    bool folded;
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

extern std::vector<std::string> cards;

extern std::unordered_map<char, int> cardToNum;

bool compare(Player p1, Player p2);

#endif