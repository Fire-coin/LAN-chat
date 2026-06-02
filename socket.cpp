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

/* Parameter is a message to be sent to another socket.
 * A single 'packet' contains a filename (nothing if simple text message),
 * size of data and data itself (actual payload).
 *
 * It is not guaranteed for everything to be sent in single send, so
 * it iterates until everything is sent
 *
 * Returns 0 on success and 1 on failure.
 * */
int ConnectionSock::send(std::string msg) {
  std::string message;
  size_t msgSize = msg.size();
  std::cout << msgSize << std::endl;
  message.append(reinterpret_cast<const char*>(&msgSize), sizeof(size_t)); 
  message.append(128, '\0'); // No filename for text message
  message.append(msg);
  
  int n = -1;
  while (msgSize > 0) {
    n = ::send(clientfd, message.c_str(), message.size(), 0);
    if (n < 0) {
      perror("Error while sending message");
      return 1;
    }
    msgSize -= n;
  }

  return 0;
}
// TODO Try to recieve everything as a single packet in order to reduce lines of code written

/* Cyclically recieves every part of the packet sent by send method of other socket.*/
int ConnectionSock::recieve(std::string& msg) {
  size_t length = 0;
  std::string filename;
  std::string data;
  
  int n = -1;

  int lengthBytes = sizeof(size_t);
  int lengthRecieved = 0;
  char* lengthBuffer = new char[lengthBytes];
  // Repeatedly recieving exactly lengthBytes bytes to determine length of message 
  while (lengthBytes > lengthRecieved) {
    n = recv(clientfd, &lengthBuffer[lengthRecieved], lengthBytes - lengthRecieved, 0);
    if (n < 0) {
      perror("Error while recieving length of message");
      return 1;
    }
    lengthRecieved += n;
  }
  
  length = *reinterpret_cast<size_t*>(lengthBuffer);
  std::cout << "Recieved size: " << length << std::endl;
  
  delete[] lengthBuffer;
  
  int filenameRecieved = 0;
  char buffer[128];
  // Repeatedly recieving until 128 bytes of filename have been recieved
  while (filenameRecieved < 128) {
    // Get the filename of the recieved file
    n = recv(clientfd, &buffer[filenameRecieved], 128 - filenameRecieved, 0); 
    if (n < 0) {
      perror("Error while recieving filename of message");
      return 1;
    }
    filenameRecieved += n;
  }
  filename.append(buffer, 128);

  std::cout << "Recieved filename: " << filename << std::endl;

  
  char* dataBuffer = new char[length];
  int dataRecieved = 0;
  // Repeatedly recieving until length bytes of data(payload) have been recieved
  while (dataRecieved < length) {
    n = recv(clientfd, &dataBuffer[dataRecieved], length - dataRecieved, 0);
    if (n < 0) {
      perror("Error while recieving data of message");
      return 1;
    }
    dataRecieved += n;
  }

  msg = std::string(dataBuffer, length);
  
  delete[] dataBuffer;

  return 0;
}

void ConnectionSock::close() {
  ::close(this->clientfd);
}


bool ConnectionSock::exists() {
  return !(this->clientfd < 0);
}
