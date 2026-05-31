#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <sys/socket.h>
#include <netinet/in.h>


class ConnectionSock {
  int clientfd;

  public:
  ConnectionSock(int cfd);

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
