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
#include <sys/epoll.h>
#include "UI.hpp"



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
  // TODO calculate a private broadcast adrress
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
  message.append((this->nickname == "") ? "unknown" : this->nickname);
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

MonitorSock::MonitorSock(int epollFd) {
  this->epollFd = epollFd;
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
ConnectionSock* MonitorSock::accept() {
  socklen_t cliLen = sizeof(clientAddr);

  clientfd = ::accept(this->serverfd, (struct sockaddr* ) &clientAddr, &cliLen);
  std::string IP = inet_ntoa(clientAddr.sin_addr);

  ConnectionSock* sock = new ConnectionSock(clientfd, IP, this->epollFd);
  return sock;
}

// It will be user's responsibility to close all Connection sockets created from this Monitor socket
void MonitorSock::close() {
  ::close(this->serverfd);
  this->clientfd = -1;
}

bool MonitorSock::exists() {
  return !(this->serverfd < 0);
}

ConnectionSock::ConnectionSock(int cfd, std::string& IPOrHost, int epollFd) {
  this->clientfd = cfd;
  this->IPOrHost = IPOrHost;
  this->isEpollout = false;
  this->epollFd = epollFd;
}

ConnectionSock::ConnectionSock(std::string& IPOrHost, std::string port, int epollFd) {
  struct addrinfo  hints;
  struct addrinfo  *result, *rp;
  int s;
  
  this->port = port;
  this->IPOrHost = IPOrHost;
  this->isEpollout = false;
  this->epollFd = epollFd;

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


int ConnectionSock::_sendPart(void* buffer, uint64_t length) {
  uint64_t transmitted = 0;
  int64_t n = -1;
  while (transmitted < length) {
    n = ::send(this->clientfd, static_cast<char*>(buffer) + transmitted, length - transmitted, 0);
    if (n == -1) {
      if (errno == EAGAIN)
        return 0;

      return -1;
    }
    transmitted += n;
  }

  return transmitted;
}

int ConnectionSock::send() {
  msgBufferMutex.lock();
  // Getting next message to be sent from buffer
  if (sendMsgBuffer.empty()) {
    msgBufferMutex.unlock();
    return 0;
  }

  auto& bufMsg = this->sendMsgBuffer.front();
  std::string& message = bufMsg.first;
  uint64_t& offset = bufMsg.second;
  
  uint64_t msgSize = message.size();

  int64_t n = this->_sendPart(message.data() + offset, msgSize - offset);
  
  // Whole message was sent
  if (offset + n == msgSize) {
    this->sendMsgBuffer.pop();
    // All the messages from the buffer have been sent, no need to track write events
    if (sendMsgBuffer.empty()) {
      this->isEpollout = false;
      struct epoll_event event;
      event.events = EPOLLIN | EPOLLET;
      event.data.fd = this->clientfd;
      
      msgBufferMutex.unlock();
      return epoll_ctl(this->epollFd, EPOLL_CTL_MOD, this->clientfd, &event);
    }
    // Call to send another message until either buffer is empty or send buffer is full
    msgBufferMutex.unlock();
    return this->send();
  }
  // Shifting offset
  offset += n;
  // There is still message in buffer, tell epoll to track write events
  if (!this->isEpollout) {
    this->isEpollout = true;
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.fd = this->clientfd;
    
    msgBufferMutex.unlock();
    return epoll_ctl(this->epollFd, EPOLL_CTL_MOD, this->clientfd, &event);
  }

  msgBufferMutex.unlock();
  return 0;
}

/* Pushes the message to be sent into queue, then calls a method to send the data */
int ConnectionSock::sendMsg(Msg& msg) {
  std::string message;
  int64_t dataSize = msg.data.size();
  int16_t filenameSize = msg.filename.size();
  // Adding header to the message
  message += std::string(reinterpret_cast<char*>(&dataSize), sizeof(int64_t));
  message += std::string(reinterpret_cast<char*>(&filenameSize), sizeof(int16_t));
  // Adding the payload of message
  message += msg.filename;
  message += msg.data;

  std::pair<std::string, int64_t> bufMsg = std::pair<std::string, int64_t>(message, 0);
  msgBufferMutex.lock();
  this->sendMsgBuffer.push(bufMsg);
  msgBufferMutex.unlock();
  return this->send();
}

/* Cyclically recieves every part of the packet sent by send method of other socket.
* Works for non-blocking socket */
int ConnectionSock::_recievePart(void* buffer, uint64_t length) {
  uint64_t recieved = 0;
  int64_t n = -1;
  while (recieved < length) {
    n = recv(this->clientfd, static_cast<char*>(buffer) + recieved, length - recieved, 0);
    if (n == 0) 
      return -2; // Indicates that file descriptor was closed
                
    if (n == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // All the data was read from file descriptor
        return recieved;
      }
      // If errno was not EAGAIN, an error has occured
      return -1;
    }
    recieved += n;
  }
  return recieved;
}
/* Return 0 when Msg is filled with data
 * -1 on error
 *  1 when no error but message not filled with data
 *  2 when file descriptor was closed*/
int ConnectionSock::recieve(Msg& msg) {
  int headerSize = sizeof(int64_t) + sizeof(int16_t);
  if (!this->rawMessageStarted) {
    memset(&this->rawMessage.header, 0, headerSize);
    this->rawMessage.data.clear();
    this->rawMessage.offset = 0;
    this->rawMessageStarted = true;
  }

  int64_t& offset = this->rawMessage.offset;
  Header& header = this->rawMessage.header;
  int64_t n = -1;
  // Recieve header info
  while (offset < headerSize) {
    n = this->_recievePart((char*)&header + offset, headerSize - offset);
    //displayError(std::to_string(header.dataSize) + " " + std::to_string(header.filenameSize) + "\n" + std::to_string(offset) + " " + std::to_string(n));
    if (n == -1) 
      return -1;
    if (n == -2)
      return 2;
    if (n == 0)
      return 1;

    offset += n;
  }
  // Am I sure this isnt Java?
  int64_t totalSize = header.filenameSize + header.dataSize;
  if (this->rawMessage.data.size() < totalSize)
    this->rawMessage.data.resize(totalSize);
  while (offset < totalSize + headerSize) {
    n = this->_recievePart(this->rawMessage.data.data() + offset - headerSize, totalSize - (offset- headerSize));
    if (n == -1)
      return -1;
    if (n == -2)
      return 2;
    if (n == 0)
      return 1;
    
    offset += n;
  }
  msg.filename.clear();
  msg.data.clear();

  if (header.filenameSize != 0) {
    msg.filename = std::string(this->rawMessage.data.data(), header.filenameSize);
  }
  msg.data = std::string(this->rawMessage.data.data() + header.filenameSize, header.dataSize);
  this->rawMessageStarted = false;
  return 0;
}

void ConnectionSock::close() {
  ::close(this->clientfd);
  this->clientfd = -1;
}


bool ConnectionSock::exists() {
  return !(this->clientfd < 0);
}

std::string ConnectionSock::getPeerIP() {
  return this->IPOrHost;
}
