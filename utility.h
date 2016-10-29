#ifndef UTILITY_H_INCLUDED
#define UTILITY_H_INCLUDED

#include <iostream>
#include <vector>
#include "config.h"

using namespace std;

vector<string> split(char *buf);

void set_unblocking(int fd);
void set_blocking(int fd);

void epfd_add(int epollfd, int fd);
void epfd_del(int epollfd, int fd);

void sig_add(int sig, void (* handler)(int));

#endif
