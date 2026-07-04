#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <fstream>
#include <queue>
#include <mutex>

constexpr int MAX_UDP_PACKET_SIZE = 128;
constexpr int MAX_NICKNAME_LENGTH = 32;
constexpr int MAX_PACKAGE_CHUNK = 20000;

struct Msg {
  std::string filename;
  std::string data;
};

class UDPDiscoverySock {
  int sockFd;
  uint16_t portNum;
  uint16_t appPortNum;
  std::string nickname;
  struct sockaddr_in broadcastAddr, transmitAddr ,senderAddr;


  public:
  UDPDiscoverySock(std::string nickname);
  UDPDiscoverySock();
  
  std::string getBroadcastAddr();
  int bind(uint16_t portNum, uint16_t appPortNum);
  int sendPresence(int delay);
  int recievePacket(std::string& senderIP, std::string& nickname, uint16_t& portNum, int delay);
  void changeNickname(std::string newNickname);
};

class ConnectionSock {
  std::string port, IPOrHost;

  std::queue<std::pair<std::string, uint64_t>> sendMsgBuffer;
  bool isEpollout;
  std::mutex msgBufferMutex;
  int epollFd;
  bool rawMessageStarted;

  int _recievePart(void* buffer, uint64_t length);
  int _sendPart(void* buffer, uint64_t length);

  public:
  struct Header {
    int64_t dataSize;
    int16_t filenameSize;
  };

  struct RawMsg {
    Header header;
    std::string data;
    std::string filename;
    int64_t offset;
    int64_t curOffset;
  };
  RawMsg rawMessage;

  int clientfd;
  ConnectionSock() : clientfd(-1) {};
  ConnectionSock(int cfd, std::string& IPOrHost, int epollFd);
  ConnectionSock(std::string& IPOrHost, std::string port, int epollFd); 
  
  int connect();
  int sendMsg(Msg& msg);
  int send();
  int sendFile(std::fstream& file, std::string& filename);
  int recieve(Msg& msg);
  bool exists();
  void close();
  std::string getPeerIP();
};


class MonitorSock {
  private:
  int clientfd, port; // Connecting socket file descriptor and port number
  struct sockaddr_in serverAddr, clientAddr;
  int epollFd;
  
  public:
  int serverfd; // Listening socket file descriptor  
  MonitorSock(int epollFd); // Default address is set to INADDR_ANY
  int bind(int portNumber);
  void listen();
  ConnectionSock* accept();
  void close();
  bool exists();


};

#endif
#pragma once
