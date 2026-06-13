#include "socket.hpp"
#include <cstring> // memset
#include <unistd.h> // close
#include <stdlib.h> // perror
#include <sys/types.h> 
#include <netdb.h>
#include <iostream>
#include <cstdint>
#include <vector>
#include <sstream>
#include <arpa/inet.h>
#include <thread>
#include <chrono>



//TODO GLOBALLY put htonl, htons and custom one for 8 bytes everywhere when sending port numbers and numbers

UDPDiscoverySock::UDPDiscoverySock() {
  this->sockFd = socket(AF_INET, SOCK_DGRAM, 0);
  const int enable = 1;
  // TODO add error checking here
  setsockopt(this->sockFd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
  setsockopt(this->sockFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  memset(&this->broadcastAddr, 0, sizeof(this->broadcastAddr));
}

UDPDiscoverySock::UDPDiscoverySock(std::string nickname) {
  this->sockFd = socket(AF_INET, SOCK_DGRAM, 0);
  const int enable = 1;
  // TODO add error checking here
  setsockopt(this->sockFd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
  setsockopt(this->sockFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  memset(&this->broadcastAddr, 0, sizeof(this->broadcastAddr));
  this->changeNickname(nickname);
}

// inspired by https://github.com/Johannes4Linux/linux_socket_examples/blob/main/udp_client.c
int UDPDiscoverySock::bind(int portNum) {
  this->portNum = portNum;
  this->broadcastAddr.sin_family = AF_INET; // IPv4
  this->broadcastAddr.sin_port = htons(portNum); 
  inet_aton("255.255.255.255", &this->broadcastAddr.sin_addr);

  //TODO move to other place probably
  this->transmitAddr.sin_family = AF_INET;
  this->transmitAddr.sin_port = htons(portNum);
  this->transmitAddr.sin_addr.s_addr = INADDR_ANY;

  return ::bind(this->sockFd, (struct sockaddr* ) &this->transmitAddr, sizeof(this->transmitAddr));
}


int UDPDiscoverySock::sendPresence(int delay) {
  std::string message;
  // Message will be in form LAN-chat|<nickname>|<portNum>
  // where <nickname> means value of nickname
  message.append("LAN-chat|");
  message.append((nickname == "") ? "unknown" : nickname);
  message.append("|");
  message.append(std::to_string(this->portNum));

  socklen_t broadcastLen = sizeof(this->broadcastAddr);
  
  int n = sendto(this->sockFd, message.data(), message.size(), 0, (struct sockaddr*) &this->broadcastAddr, broadcastLen);
  std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  return n;
}


int UDPDiscoverySock::recievePacket(std::string& senderIP, std::string& nickname, int delay) {
  std::vector<char> buffer(MAX_UDP_PACKET_SIZE);
  socklen_t senderLen = sizeof(this->senderAddr);
  int n = recvfrom(this->sockFd, buffer.data(), MAX_UDP_PACKET_SIZE, 0, (struct sockaddr*) &this->senderAddr, &senderLen);
  std::string msg(buffer.data(), n);
  std::stringstream ss(msg);
  if (msg.find_first_of('|', 0) == msg.npos)
    return -2;
  std::string appVer, nick, port;
  std::getline(ss, appVer, '|');
  if (appVer != "LAN-chat")
    return -2;
  std::getline(ss, nick, '|');
  std::getline(ss, port, '|');
  if (port != std::to_string(this->portNum))
    return -2;

  // TODO add check to see if the packet arrived from the other LAN-chat Peer
  // TODO also add nickname separation from packet
  senderIP = inet_ntoa(this->senderAddr.sin_addr);
  nickname = nick;
  std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  return n;
}
// TODO add checking for max length of nickname
void UDPDiscoverySock::changeNickname(std::string newNickname) {
  this->nickname = newNickname;
}

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
  this->clientfd = -1;
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
    n = ::send(this->clientfd, static_cast<char*>(buffer) + transmitted, length - transmitted, 0);
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
  //std::cout << "Sending filename length of: " << filenameLength << " and filename is: " << filename << std::endl;
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
  //std::cout << msgSize << std::endl;
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
    n = recv(this->clientfd, static_cast<char*>(buffer) + recieved, length - recieved, 0);
    if (n < 0)
      return 1;
    recieved += n;
  }
  return 0;
}

/* Cyclically recieves every part of the packet sent by send method of other socket.*/
int ConnectionSock::recieve(Msg& msg) {
  uint64_t length = 0;
  uint16_t filenameLength = 0;
  std::string filename;
  std::string data;
  
  int n = -1;

  
  n = this->_recievePart(&length, sizeof(length));
  if (n < 0) {
    std::cerr << "Error while recieving length of data\n";
    return 1;
  }
  //std::cout << "Recieved length: " << length << std::endl;
  
  n = this->_recievePart(&filenameLength, sizeof(filenameLength));
  if (n < 0) {
    std::cerr << "Error while recieving length of filename\n";
    return 1;
  }
  //std::cout << "Recieved filename length: " << filenameLength << std::endl;
  
  if (filenameLength > 0) { // Reading filename only if file was sent
    std::vector<char> filenameBuffer(filenameLength);

    n = this->_recievePart(filenameBuffer.data(), filenameLength);
    if (n < 0) {
      std::cerr << "Error while recieving filename\n";
      return 1;
    }
    msg.filename = std::string(filenameBuffer.data(), filenameLength);
    //std::cout << "Recieved filename: " << msg.filename << std::endl;
  } else
    msg.filename = "";

  std::vector<char> dataBuffer(length);
  n = this->_recievePart(dataBuffer.data(), length);
  if (n < 0) {
    std::cerr << "Error while recieving data\n";
    return 1;
  }
  msg.data = std::string(dataBuffer.data(), length);

  return 0;
}

void ConnectionSock::close() {
  ::close(this->clientfd);
  this->clientfd = -1;
}


bool ConnectionSock::exists() {
  return !(this->clientfd < 0);
}
