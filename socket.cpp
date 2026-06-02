#include "socket.hpp"
#include <cstring> // memset
#include <unistd.h> // close
#include <stdlib.h> // perror
#include <sys/types.h> 
#include <netdb.h>
#include <iostream>
MonitorSock::MonitorSock() {
  // IPv4 TCP socket
  this->serverfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&serverAddr, 0, sizeof(serverAddr));
}

int MonitorSock::bind(int portNum) {
  this->port = portNum;
  serverAddr.sin_family = AF_INET; // IPv4
  serverAddr.sin_port = htons(portNum);
  serverAddr.sin_addr.s_addr = INADDR_ANY; // Any addres
  
  return ::bind(this->serverfd, (struct sockaddr* ) &serverAddr, sizeof(serverAddr));
}

void MonitorSock::listen() {
  ::listen(this->serverfd, 5);
}

/* Returns a Connection socket which can communicate with other peer.
 * One must check if it exists before using it. */
ConnectionSock MonitorSock::accept() {
  socklen_t cliLen = sizeof(clientAddr);

  clientfd = ::accept(this->serverfd, (struct sockaddr* ) &clientAddr, &cliLen);
  
  return ConnectionSock(clientfd);
}

// It will be user's responsibility to close all Connection sockets created from this Monitor socket
void MonitorSock::close() {
  ::close(this->serverfd);
}

bool MonitorSock::exists() {
  return !(this->serverfd < 0);
}

ConnectionSock::ConnectionSock(int cfd) {
  this->clientfd = cfd;
}

ConnectionSock::ConnectionSock(std::string IPOrHost, std::string port) {
  struct addrinfo  hints;
  struct addrinfo  *result, *rp;
  int s;
  
  this->port = port;
  this->IPOrHost = IPOrHost;

  memset(&hints, 0, sizeof(hints));

  hints.ai_family = AF_INET;    
  hints.ai_socktype = SOCK_STREAM; 
  hints.ai_flags = 0;
  hints.ai_protocol = 0;
  s =  getaddrinfo(IPOrHost.c_str(), port.c_str(), &hints, &result);
  if (s < 0)
    perror("Error while creating Connection socket");


  for (rp = result; rp != NULL; rp = rp->ai_next) {
    this->clientfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (this->clientfd == -1)
      continue;

    if (connect(this->clientfd , rp->ai_addr, rp->ai_addrlen) != -1)
      break;                  /* Success */

    ::close(this->clientfd);
  }
  
  freeaddrinfo(result);

  if (rp == NULL)
    fprintf(stderr, "Could not connect\n");
}
// TODO make the display of errors more consistent
int ConnectionSock::send(std::string msg) {
  std::string message;
  size_t msgSize = msg.size();
  std::cout << msgSize << std::endl;
  message.append(reinterpret_cast<const char*>(&msgSize), sizeof(size_t)); 
  message.append(128, '\0'); // No filename for text message
  message.append(msg);
  
  return ::send(clientfd, message.c_str(), message.size(), 0);
}

// TODO the number of bytes written can be less than specified,
// so implement cyclic sending of all the data
// Current limit for testing will be set to 1024
int ConnectionSock::recieve(std::string& msg) {
  size_t length = 0;
  std::string filename;
  std::string data;

  int n = recv(clientfd, reinterpret_cast<char*>(&length), sizeof(size_t), 0);
  std::cout << "Recieved size: " << length << std::endl;
  
  char buffer[128];
  n = recv(clientfd, buffer, 128, 0); // Get the filename of the recieved file

  filename.append(buffer, 128);

  char* dataBuffer = new char[length];
  n = recv(clientfd, dataBuffer, length, 0);

  msg = std::string(dataBuffer, length);

  //char buffer[1024];
  //memset(&buffer, 0, sizeof(buffer));

  //n = recv(clientfd, buffer, sizeof(buffer), 0);
  //
  //if (n < 0)
  //  perror("Error recieving from socket");

  //msg = std::string(buffer, n);

  return n;
}

void ConnectionSock::close() {
  ::close(this->clientfd);
}


bool ConnectionSock::exists() {
  return !(this->clientfd < 0);
}
