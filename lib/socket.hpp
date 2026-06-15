#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <fstream>

constexpr int MAX_UDP_PACKET_SIZE = 128;

struct Msg {
  std::string filename;
  std::string data;
};

class UDPDiscoverySock {
  int sockFd;
  int portNum;
  std::string nickname;
  struct sockaddr_in broadcastAddr, transmitAddr ,senderAddr;


  public:
  UDPDiscoverySock(std::string nickname);
  UDPDiscoverySock();
  
  int bind(int portNum);
  int sendPresence(int delay);
  int recievePacket(std::string& senderIP, std::string& nickname, int delay);
  void changeNickname(std::string newNickname);
};

class ConnectionSock {
  int clientfd;
  std::string port, IPOrHost;

  int _recievePart(void* buffer, uint64_t length);
  int _sendPart(void* buffer, uint64_t length);

  public:
  ConnectionSock() : clientfd(-1) {};
  ConnectionSock(int cfd, std::string IPOrHost);
  ConnectionSock(std::string IPOrHost, std::string port); 

  int send(std::string msg);
  int sendFile(std::fstream& file, std::string& filename);
  int recieve(Msg& msg);
  bool exists();
  void close();
  std::string getPeerIP();
};


class MonitorSock {
  private:
  int serverfd; // Listening socket file descriptor  
  int clientfd, port; // Connecting socket file descriptor and port number
  struct sockaddr_in serverAddr, clientAddr;
  
  public:
  // TODO add a bind function which will accept IP address *Only if needed
  MonitorSock(); // Default address is set to INADDR_ANY
  int bind(int portNumber);
  void listen();
  ConnectionSock accept();
  void close();
  bool exists();


};





#endif
#pragma once
