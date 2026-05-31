#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "socket.hpp"

void error(const char* msg) {
  perror(msg);
  exit(1);
}


int main() {

  std::cout << "Start of the Test\n";
  
  std::cout << "Select to be server(0) or client(1)\n";
  int choice;
  std::cin >> choice;
  if (choice != 0 && choice != 1)
    return 0;

  int portNum;
  char buffer[256];
  portNum = 555555;
  int n; // Number of bytes written / read
  

  if (choice == 0) {
    int sfd, cfd; // File descriptors of server and client sockets and port number
    struct sockaddr_in serv_addr, cli_addr;
    
    portNum = 55555; // Custom port number, hopefully not occupied

    sfd = socket(AF_INET, SOCK_STREAM, 0); // Creating a tcp socket
    
    if (sfd < 0)
      error("Error opening socket");
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portNum);
    serv_addr.sin_addr.s_addr = INADDR_ANY; 

    if (bind(sfd, (struct sockaddr* ) &serv_addr, sizeof(serv_addr)) < 0)
      error("Error binding socket");

    listen(sfd, 5);
    
    socklen_t cliLen = sizeof(cli_addr);

    cfd = accept(sfd, (struct sockaddr*) &cli_addr, &cliLen);
    
    if (cfd < 0)
      error("Error accepting socket");

    memset(&buffer, 0, sizeof(buffer));
    n = recv(cfd, buffer, sizeof(buffer), 0);
    if (n < 0)
      error("Error reading from socket");
    printf("Recieved message %s\n", buffer);
    n = send(cfd, "Message recieved", 16, 0);
    if (n < 0)
      error("Error writing to socket");
    close(cfd);
    close(sfd);

  } else {
    int sfd, s;

    struct addrinfo  hints;
    struct addrinfo  *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;    
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    
    // FIrst parameter can be either ip adress or hostname e.g: localhost
    s = getaddrinfo("localhost", "55555", &hints, &result);

    for (rp = result; rp != NULL; rp = rp->ai_next) {
     sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
     if (sfd == -1)
       continue;

     if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
       break;                  /* Success */

     close(sfd);
    }

    freeaddrinfo(result);

    if (rp == NULL) {               /* No address succeeded */
      fprintf(stderr, "Could not connect\n");
      exit(EXIT_FAILURE);
    }
    
    std::string msg;
    std::cout << "Enter message: ";
    std::getline(std::cin, msg); // To free up the stream because of the first input of integer
    std::getline(std::cin, msg);

    std::cout << "Given message: " << msg << std::endl;

    n = send(sfd, msg.c_str(), msg.size(), 0);
    if (n < 0)
      error("Error sending to socket");
    
    memset(buffer, 0, sizeof(buffer));

    n = recv(sfd, buffer, sizeof(buffer), 0);
    
    if (n < 0)
      error("Error recieving from socket");

    close(sfd);
  }
  return 0;
}
