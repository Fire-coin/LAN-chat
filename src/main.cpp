#include "socket.hpp"
#include "UI.hpp"
#include "socketFuncs.hpp"
#include "fileFuncs.hpp"
#include "peer.hpp"
#include <iostream> // std::cout, std::cin
#include <vector> // std::vector
#include <future> // std::future
#include <chrono> // for sleep_for function
#include <string> // std::string
#include <mutex> // std::mutex
#include <utility> // for std::pair


void error(const char* msg) {
  perror(msg);
  exit(1);
}

int main() {
  
  int choice;
  int portNum = 55555;
  int discoveryPortNum = 5000;
  // Both added to supress compiler warnings
  std::future<void> _monitorRequest;
  std::future<void> _sendingRequest;

  // Currently doing test for UDP discovery
  // TODO run this rather on a thread to be able to cancel it
  std::future<void> _discoverRequest = std::async(std::launch::async, [discoveryPortNum]() { discoverPeers(discoveryPortNum); });
  
  std::string selectedIP;
  std::string nick;
  HOME_OPTIONS selectedOption;
  bool showUI = true;
  beginUI();
  while (showUI) {
    selectedOption = displayHomeScreen();
    switch (selectedOption) {
      case NEW_CHAT:
        selectedIP = displayNewChatScreen();
        break;
      case CHATS:
        // Populate vector for test case
        connectedIPs.emplace_back("198.168.10.12", "random guy");
        connectedIPs.emplace_back("198.168.10.54", "Ibuprofen");
        selectedIP = displayChatsScreen(connectedIPs);
        break;
      case CHANGE_NICKNAME:
        nick = displayChangeNicknameScreen(nickname);
        if (nick == "~|NO_change|~")
          break;
        nickMutex.lock();
        if (nick != nickname) {
          nickname = nick;
          changeNickname = true;
        }
        nickMutex.unlock();
        break;
      case EXIT:
        showUI = false;
        break;
    }
  }
  endUI();
  


  std::cout << "Do you want to start monitoring(1 / 0): ";
  std::cin >> choice;
  
  if (choice == 1)
  _monitorRequest = std::async(std::launch::async, [portNum]() { monitor(portNum); });
 
  //TODO run this on a separate thread so it can be cancelled unlike async
  while (doListening && !isConnected) {
    std::cout << "Start connection(0 / 1): ";
    std::cin >> choice;
    
    if (!isConnected && choice == 1) {
      std::cout << "Available devices: \n";
      showPeers();
      std::string ip;
      std::cout << "Enter IP to connect to: ";
      std::getline(std::cin, ip);
      std::getline(std::cin, ip);
      _sendingRequest = std::async(std::launch::async, [portNum, ip]() { establishConnection(ip, portNum); });
    }
    else if (choice == 0 && !isConnected) {
      std::cout << "Exit app (1 / 0)?: ";
      std::cin >> choice;
    
      if (choice == 1)
        exit(0);
    }
  }
  
  
  return 0;
}
