#ifndef SOCKET_FUNCS_HPP
#define SOCKET_FUNCS_HPP

#include "socket.hpp"
#include <string>

bool sendMessage(ConnectionSock& socket, std::string msg);
bool sendFile(ConnectionSock& socket, std::string filepath);
int recieve(ConnectionSock& socket, std::string& message);


void monitor(int portNum);
void establishConnection(ConnectionSock conSock);
void establishConnection(std::string IPOrHost, int portNum);
// TODO set all global vaiables to false before exiting
// Global variables used to control async processes
extern bool acceptConnection;
extern bool doConnection;
extern bool doListening;
extern bool isConnected;
extern bool doPeerDiscovery;


#endif
#pragma once
