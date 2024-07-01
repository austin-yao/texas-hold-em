#ifndef MAIN_H
#define MAIN_H

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

#include "game.h"
#include "helper.h"
#include "models.h"

void processCommandLine(int argc, char *argv[]);
int main(int argc, char *argv[]);

#endif