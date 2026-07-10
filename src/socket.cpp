#include "socket.hpp"
#include "appErrors.hpp"
#include "UI.hpp"
#include <unistd.h> // close
#include <sys/types.h> 
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <cstring> // memset
#include <cstdint>
#include <vector>
#include <thread> // std::this_thread
#include <sstream>
#include <chrono>
#include <cassert>


/* User should always check socket if exists() method */
UDPDiscoverySock::UDPDiscoverySock() {
  this->sockFd = socket(AF_INET, SOCK_DGRAM, 0);
  const int enable = 1;
  setsockopt(this->sockFd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
  setsockopt(this->sockFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  memset(&this->broadcastAddr, 0, sizeof(this->broadcastAddr));
}

/* User should always check socket if exists() method */
UDPDiscoverySock::UDPDiscoverySock(std::string nickname) {
  this->sockFd = socket(AF_INET, SOCK_DGRAM, 0);
  const int enable = 1;
  setsockopt(this->sockFd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int));
  setsockopt(this->sockFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  memset(&this->broadcastAddr, 0, sizeof(this->broadcastAddr));
  this->changeNickname(nickname);
}
/* Gets private broadcast address using ifaddrs */
std::string UDPDiscoverySock::getBroadcastAddr() {
  struct ifaddrs *ifaddr;
  struct sockaddr_in* broadcastAddr;
  int family, s;
  
  if (getifaddrs(&ifaddr) != 0) {
    pushError("getifaddrs failed; UDPDiscoverySock::getBroadcastAddr", LCE_SYS_CALL);
  }
  /* Going through the linked list of addresses of current device. */
  for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
    /* Check for IPv4 only */
    if (ifa->ifa_addr->sa_family == AF_INET) {
      struct sockaddr_in* addr = (struct sockaddr_in*) ifa->ifa_addr;
      /* Checks and continues if address is loopback (ussually 127.0.0.1) */
      if (ifa->ifa_flags & IFF_LOOPBACK)
        continue;

      /* If current IP has broadcast address, return it */
      if (ifa->ifa_flags & IFF_BROADCAST) {
        broadcastAddr = (struct sockaddr_in* ) ifa->ifa_broadaddr;
        freeifaddrs(ifaddr);
        return inet_ntoa(broadcastAddr->sin_addr);
      }
    }
  }
  freeifaddrs(ifaddr);
  /* No IP has broadcast address, most probably the device is not connected to network */
  return "";
}
/* Binds the socket to calculated broadcast address */
// inspired by https://github.com/Johannes4Linux/linux_socket_examples/blob/main/udp_client.c
int UDPDiscoverySock::bind(uint16_t portNum, uint16_t appPortNum) {
  this->portNum = portNum;
  this->appPortNum = appPortNum;

  this->broadcastAddr.sin_family = AF_INET; // IPv4
  this->broadcastAddr.sin_port = htons(portNum); 
  
  std::string privateBroadAddr = this->getBroadcastAddr();
  if (privateBroadAddr == "") {
    //privateBroadAddr = "255.255.255.255";
    return LCE_BIND;
  }

  inet_aton(privateBroadAddr.c_str(), &this->broadcastAddr.sin_addr);

  this->transmitAddr.sin_family = AF_INET;
  this->transmitAddr.sin_port = htons(portNum);
  this->transmitAddr.sin_addr.s_addr = INADDR_ANY;

  if (::bind(this->sockFd, (struct sockaddr* ) &this->transmitAddr, sizeof(this->transmitAddr)) == -1)
    return LCE_BIND;
  return 0;
}

/* Sends a packet to the address to which it was binded to.
 * The packet is in the form LAN-chat|<nickname>|<portNum> */
int UDPDiscoverySock::sendPresence(int delay) {
  std::string message;
  message.append("LAN-chat|");
  message.append((this->nickname == "") ? "unknown" : this->nickname);
  message.append("|");
  message.append(std::to_string(this->appPortNum));

  socklen_t broadcastLen = sizeof(this->broadcastAddr);
  
  int n = sendto(this->sockFd, message.data(), message.size(), 0, (struct sockaddr*) &this->broadcastAddr, broadcastLen);
  std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  if (n == -1)
    return LCE_SEND;
  return n;
}

/* Recieves packet from address (private broadcast one) and it sets the
 * parameters IP and nickname from recieved IP and nickname respectively */
int UDPDiscoverySock::recievePacket(std::string& senderIP, std::string& nickname, uint16_t& peerPortNum, int delay) {
  std::vector<char> buffer(MAX_UDP_PACKET_SIZE);
  socklen_t senderLen = sizeof(this->senderAddr);
  int n = recvfrom(this->sockFd, buffer.data(), MAX_UDP_PACKET_SIZE, 0, (struct sockaddr*) &this->senderAddr, &senderLen);
  if (n < 0) {
    return n;
  }

  std::string msg(buffer.data(), n);
  std::stringstream ss(msg);
  if (msg.find_first_of('|', 0) == msg.npos)
    return LCE_BAD_PACKET;
  /* Getting the app version from packet */
  std::string appVer, nick, port;
  std::getline(ss, appVer, '|');
  if (appVer != "LAN-chat")
    return LCE_BAD_PACKET;
  /* Getting nickname and port number from packet */
  std::getline(ss, nick, '|');
  std::getline(ss, port, '|');
  int portInt = std::stoi(port);
  if (portInt >= static_cast<int>(UINT16_MAX) || portInt < 0) {
    return LCE_BAD_PACKET;
  }
  
  senderIP = inet_ntoa(this->senderAddr.sin_addr);
  
  /* Trunckating the nickname to fit the margin */
  if (nick.size() > MAX_NICKNAME_LENGTH)
    nickname = nick.substr(0, MAX_NICKNAME_LENGTH);
  else
    nickname = nick;

  peerPortNum = static_cast<uint16_t>(portInt);

  std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  if (n == -1)
    return LCE_SEND;
  return n;
}

void UDPDiscoverySock::changeNickname(std::string newNickname) {
  /* Trunckating the nickname to fit the margin */
  if (newNickname.size() > MAX_NICKNAME_LENGTH) {
    this->nickname = newNickname.substr(0, MAX_NICKNAME_LENGTH);
    return;
  }

  this->nickname = newNickname;
}

/* User should always check socket with exists() method */
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

  if (::bind(this->serverfd, (struct sockaddr* ) &serverAddr, sizeof(serverAddr)) == -1) {
    return LCE_BIND;
  }
  return 0;
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

/* User should always check socket with exists() method */
ConnectionSock::ConnectionSock(int cfd, std::string& IPOrHost, int epollFd) {
  this->clientfd = cfd;
  this->IPOrHost = IPOrHost;
  this->isEpollout = false;
  this->epollFd = epollFd;
}

ConnectionSock::ConnectionSock(std::string& IPOrHost, std::string port, int epollFd) {
  this->clientfd = socket(AF_INET, SOCK_STREAM, 0);
  this->port = port;
  this->IPOrHost = IPOrHost;
  this->isEpollout = false;
  this->epollFd = epollFd;
}
/* Connects to the peer with IP and port specified in initialization.
 * This is done using getaddrinfo */
int ConnectionSock::connect() {
  struct addrinfo  hints;
  struct addrinfo  *result, *rp;
  int s;

  memset(&hints, 0, sizeof(hints));

  hints.ai_family = AF_INET;    
  hints.ai_socktype = SOCK_STREAM; 
  hints.ai_flags = 0;
  hints.ai_protocol = 0;
  s =  getaddrinfo(IPOrHost.c_str(), port.c_str(), &hints, &result);
  if (s != 0) {
    pushError(port + " " + IPOrHost, -1);
    return LCE_CONNECT;
  }


  for (rp = result; rp != NULL; rp = rp->ai_next) {
    this->clientfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (this->clientfd == -1)
      continue;

    if (::connect(this->clientfd , rp->ai_addr, rp->ai_addrlen) != -1)
      break;                  /* Success */

    ::close(this->clientfd);
  }
  
  freeaddrinfo(result);

  if (rp == NULL) {
    pushError(port + " " + IPOrHost, -1);
    return LCE_CONNECT;
  }
  return 0;
}
/* Sends given amount of bytes from buffer. Returns actual amount of bytes sent or LCE_SEND
 * if there is error with send sys call*/
int ConnectionSock::_sendPart(void* buffer, uint64_t length) {
  uint64_t transmitted = 0;
  int64_t n = -1;
  while (transmitted < length) {
    n = ::send(this->clientfd, static_cast<char*>(buffer) + transmitted, length - transmitted, 0);
    if (n == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return transmitted;

      return LCE_SEND;
    }
    transmitted += n;
  }

  return transmitted;
}
/* Sends next message from sendMsgBuffer */
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
  if (n < 0)
    return n;
  
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
      if (epoll_ctl(this->epollFd, EPOLL_CTL_MOD, this->clientfd, &event) == -1)
        return LCE_EPOLL_CTL;
      return 0;
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
    if(epoll_ctl(this->epollFd, EPOLL_CTL_MOD, this->clientfd, &event) == -1)
      return LCE_EPOLL_CTL;
    return 0;
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
      return LCE_FD_CLOSED; // Indicates that file descriptor was closed
                
    if (n == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // All the data was read from file descriptor
        return recieved;
      }
      // If errno was not EAGAIN, an error has occured
      return LCE_RECIEVE;
    }
    recieved += n;
  }
  return recieved;
}

/* Returns:
 * If there is any error from _sendPart, it will be returned
 * LCE_NOT_FULL_MSG - when non payload part of message has not arrived yet, or when only part of text message came through
 * LCE_NOT_FULL_PACKAGE - Tells that some part of file has been recieved, but it still not whole
 * LCE_FULL_PACKAGE - Whole file has already been recieved
 *
 * It fills msg.data with parts of file each time it recieves them. 
 * If message is text message, rawMessade.data holds it until it is complete, then it
 * is also stored in msg.data.
 * */
int ConnectionSock::recieve(Msg& msg) {
  int headerSize = sizeof(int64_t) + sizeof(int16_t);
  if (!this->rawMessageStarted) {
    memset(&this->rawMessage.header, 0, headerSize);
    this->rawMessage.data.clear();
    this->rawMessage.filename.clear();
    this->rawMessage.offset = 0;
    this->rawMessage.curOffset = 0;
    this->rawMessageStarted = true;
  }
  
  uint64_t& offset = this->rawMessage.offset;
  Header& header = this->rawMessage.header;
  int64_t n = -1;
  

  // Recieve header info
  while (offset < headerSize) {
    n = this->_recievePart((char*)&header + offset, headerSize - offset);
    if (n < 0)
      return n;
    
    offset += n;
    if (offset < headerSize)
      return LCE_NOT_FULL_MSG;
  }
  
  if (this->rawMessage.filename.empty()) {
    this->rawMessage.filename.resize(header.filenameSize);
  }

    
  /* Recieve filename */
  while (offset - headerSize < header.filenameSize) {
    n = this->_recievePart(this->rawMessage.filename.data() + offset - headerSize, header.filenameSize - (offset - headerSize));
    if (n < 0)
      return n;
    offset += n;
    if (offset - headerSize < header.filenameSize)
      return LCE_NOT_FULL_MSG;
  }
  
  uint64_t dataRecieved = (offset - headerSize - header.filenameSize);
  /* Creating buffer for data recieved, we recieve at once at most MAX_PACKAGE_CHUNK bytes */
  std::string recievedData(MAX_PACKAGE_CHUNK, '\0');

  n = this->_recievePart(recievedData.data(), recievedData.size());

  if (n < 0)
    return n;

  if (n == 0)
    return LCE_NOT_FULL_MSG;

  
  offset += n;

  msg.filename.clear();
  msg.data.clear();
  
  /* Resizing message to the size of recieved bytes */
  recievedData.resize(n);

  msg.filename = this->rawMessage.filename;
  /* If it is a file, then put this chunk into msg.data. 
   * If it is message, append it to rawMessage.data */
  if (!this->rawMessage.filename.empty())
    msg.data = recievedData;
  else
    this->rawMessage.data += recievedData;
  /* If offset starting from where data starts is still less than dataSize */
  if (offset - (uint64_t)headerSize - (uint64_t)header.filenameSize < header.dataSize) {
    if (this->rawMessage.filename.empty()) {
      return LCE_NOT_FULL_MSG;
    }
    return LCE_NOT_FULL_PACKAGE;
  }
  /* Function gets here, when all the data has been recieved */
  
  /* Put the stored message into msg.data */
  if (this->rawMessage.filename.empty()) {
    msg.data = this->rawMessage.data;
  }

  assert(offset == headerSize + header.filenameSize + header.dataSize);
  n = -1;

  n = this->_recievePart(&header, 1);
  assert(n <= 0);
  this->rawMessageStarted = false;
  /* This is here for safety clearing all the fields */
  memset(&this->rawMessage.header, 0, headerSize);
  this->rawMessage.data.clear();
  this->rawMessage.filename.clear();
  this->rawMessage.offset = 0;
  this->rawMessage.curOffset = 0;
  this->rawMessageStarted = true;

  return LCE_FULL_PACKAGE;
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
