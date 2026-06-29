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

int closePeerConnection(std::string IP);
void handlePeerRequestsWrapper(int portNum);
int endRequestHandling();
int setNonblocking(int sockfd);
int processFile(std::string& filepath, Msg& msg);
void establishConnection(std::string& IPOrHost);
void handlePeerRequests(int portNum);
extern std::atomic<bool> handleRequests;
extern int epollFd;

extern std::vector<ConnectionSock*> connectedSockets;
extern std::mutex connectedSocketsMutex;

#endif
#pragma once
