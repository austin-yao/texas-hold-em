#include <stdio.h>
#include <unistd.h>
#include <string>

struct Player
{
    std::string username;
    std::string connection;
    int fd;
    int stack;
};

bool compare(Player p1, Player p2);