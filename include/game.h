#ifndef GAME_H
#define GAME_H

#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <arpa/inet.h>
#include <queue>
#include <vector>
#include <thread>
#include <shared_mutex>
#include <algorithm>
#include <random>
#include <unordered_set>
#include <tuple>

#include "models.h"
#include "helper.h"

void resetPlayer(Player *player);

void resetPlayers(Hand &hand);

void resetHand(Hand &hand);

int addPlayer(Player &player);

void broadcastMessage(std::string msg);

std::vector<int> calculateBlinds(Hand &hand, Game &game);

std::vector<int> gameFinished(Hand &hand, Game &game);

std::vector<Hand> betStreet(Hand &hand, bool preflop);

void broadcastStacks(Hand &hand);

std::vector<int> handValue(std::vector<std::string> userCards);

bool handComparator(const std::tuple<std::vector<int>, Player *> &a, const std::tuple<std::vector<int>, Player *> &b);

#endif