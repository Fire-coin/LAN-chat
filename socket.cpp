#include "socket.hpp"
#include <cstring> // memset
#include <unistd.h> // close
#include <stdlib.h> // perror
#include <sys/types.h> 
#include <netdb.h>
#include <iostream>
#include <cstdint>
#include <vector>

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


int ConnectionSock::_sendPart(void* buffer, uint64_t length) {
  uint64_t transmitted = 0;
  int n = -1;
  while (transmitted < length) {
    n = ::send(this->clientfd, buffer + transmitted, length - transmitted, 0);
    if (n < 0)
      return -1;
    transmitted += n;
  }

  return 0;
}
// TODO implement htons or network file order
int ConnectionSock::sendFile(std::fstream& file, std::string& filename) {
  // Get length of file
  file.seekg(0, file.end);
  uint64_t fileLength = file.tellg();
  file.seekg(0, file.beg);
  
  // Get length of filename
  uint16_t filenameLength = filename.size();
  std::cout << "Sending filename length of: " << filenameLength << "and filename is: " << filename << std::endl;
  // Create buffer for data and read data into it
  std::vector<char> dataBuffer(fileLength);
  file.read(dataBuffer.data(), fileLength);

  file.close();
  // Sending each field separately
  this->_sendPart(&fileLength, 8);
  this->_sendPart(&filenameLength, 2);
  this->_sendPart(filename.data(), filenameLength);
  this->_sendPart(dataBuffer.data(), fileLength);

  return 0;
}

int ConnectionSock::send(std::string msg) {
  std::string message;
  uint64_t msgSize = msg.size();
  std::cout << msgSize << std::endl;
  std::string filename = "";
  uint16_t filenameLength = 0;
  

  this->_sendPart(&msgSize, 8);
  this->_sendPart(&filenameLength, 2);
  // No need for sending 0 bytes
  //this->_sendPart(filename.data(), filenameLength);
  this->_sendPart(msg.data(), msgSize);

  return 0;
}
// TODO Try to recieve everything as a single packet in order to reduce lines of code written

int ConnectionSock::_recievePart(void* buffer, uint64_t length) {
  uint64_t recieved = 0;
  int n = -1;
  while (recieved < length) {
    n = recv(this->clientfd, buffer + recieved, length - recieved, 0);
    if (n < 0)
      return 1;
    recieved += n;
  }
  return 0;
}

/* Cyclically recieves every part of the packet sent by send method of other socket.*/
int ConnectionSock::recieve(std::string& msg) {
  uint64_t length = 0;
  uint16_t filenameLength = 0;
  std::string filename;
  std::string data;
  
  int n = -1;

  
  n = this->_recievePart(&length, sizeof(length));
  if (n < 0)
    std::cerr << "Error while recieving length of data\n";
  std::cout << "Recieved length: " << length << std::endl;
  
  n = this->_recievePart(&filenameLength, sizeof(filenameLength));
  if (n < 0)
    std::cerr << "Error while recieving length of filename\n";
  std::cout << "Recieved filename length: " << filenameLength << std::endl;
  
  if (filenameLength > 0) { // Reading filename only if file was sent
    std::vector<char> filenameBuffer(filenameLength);

    n = this->_recievePart(filenameBuffer.data(), filenameLength);
    if (n < 0)
      std::cerr << "Error while recieving filename\n";
    filename = std::string(filenameBuffer.data(), filenameLength);
    std::cout << "Recieved filename: " << filename << std::endl;
  }
  std::vector<char> dataBuffer(length);
  n = this->_recievePart(dataBuffer.data(), length);
  if (n < 0)
    std::cerr << "Error while recieving data\n";
  data = std::string(dataBuffer.data(), length);

  msg = data;

  return 0;
}

void ConnectionSock::close() {
  ::close(this->clientfd);
}


bool ConnectionSock::exists() {
  return !(this->clientfd < 0);
}
