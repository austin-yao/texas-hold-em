#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <arpa/inet.h>
#include <vector>

bool do_read_help(int fd, char *buf, int len);
bool do_write(int fd, char *buf, int len);
bool do_write_cstr(int fd, char *buf, int len);
bool do_write_string(int fd, std::string msg);
std::string do_read(int fd);
std::string stringifyAddress(sockaddr_in src);
std::vector<std::string> split(std::string str, char target);

#endif