#ifndef SOCKET_FUNCS_HPP
#define SOCKET_FUNCS_HPP

#include "socket.hpp"
#include <string>
#include <vector>
#include <atomic>

int sendMessage(std::string& IP, std::string& msg);
bool _sendMessage(ConnectionSock* socket, std::string msg);
int _sendFile(ConnectionSock* socket, std::string filepath);
int recieve(ConnectionSock& socket, std::string& message);


//void monitor(int portNum);
//void establishConnection(ConnectionSock conSock);
void establishConnection(std::string IPOrHost, int portNum);
void handlePeerRequests(int portNum);
extern std::atomic<bool> handleRequests;
extern int epollFd;

extern std::vector<ConnectionSock*> connectedSockets;
// TODO set all global vaiables to false before exiting
// Global variables used to control async processes
extern bool acceptConnection;
extern bool doConnection;
extern bool doListening;
extern bool isConnected;
//extern bool doPeerDiscovery;


#endif
#pragma once
