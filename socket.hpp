#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <string>

class ConnectionSock {
  int clientfd;
  std::string port, IPOrHost;

  public:
  ConnectionSock(int cfd);
  ConnectionSock(std::string IPOrHost, std::string port); 

  int send(std::string msg);
  int recieve(std::string& msg);
  bool exists();
  void close();
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
