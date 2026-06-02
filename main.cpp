#include <iostream>
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

  int portNum = 55555;
  int n; // Number of bytes written / read
  

  if (choice == 0) {
    MonitorSock monSock = MonitorSock();
    
    if (monSock.bind(portNum))
      error("Error while monitoring socket");

    monSock.listen();

    ConnectionSock conSock = monSock.accept();
    
    if (!conSock.exists())
      error("Error while creating connection socket");

    std::string msg;
    std::getline(std::cin, msg); // Clear the buffer
    while (msg != "exit") {
      std::cout << "Enter message to send to other device: ";
      std::getline(std::cin, msg);
      n = conSock.send(msg);
      if (n < 0)
        perror("Error sending message to socket");
        
      n = conSock.recieve(msg);
      if (n == 1)
        continue;
      std::cout << "Recieved message: " << msg << std::endl;
    }

    conSock.close();
    monSock.close();

  } else {
    ConnectionSock conSock = ConnectionSock("localhost", "55555");

    if (!conSock.exists())
      error("Error connecting to Socket");
    
    std::string msg;
    std::getline(std::cin, msg);

    while (msg != "exit") {
      n = conSock.recieve(msg);
      if (n < 0)
        std::cout << "No recieved message\n";
      else
        std::cout << "Recieved message: " << msg << std::endl;

      std::cout << "Enter message to transmit: ";
      std::getline(std::cin, msg);

      n = conSock.send(msg);
      if (n < 0)
        perror("Error sending to socket");
    }

    conSock.close();

  }
  return 0;
}
