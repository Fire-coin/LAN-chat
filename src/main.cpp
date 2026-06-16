#include "socket.hpp"
#include "UI.hpp"
#include "socketFuncs.hpp"
#include "fileFuncs.hpp"
#include "peer.hpp"
#include "fileSelector.hpp"
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
  _monitorRequest = std::async(std::launch::async, [portNum]() { monitor(portNum); });
  
  std::string selectedIP;
  std::string nick;
  HOME_OPTIONS selectedOption = NO_OPTION;
  bool showUI = true;
  beginUI();
  // testing file selector
  displayFileSelector();
  while (showUI) {
    selectedOption = displayHomeScreen();
    switch (selectedOption) {
      case NEW_CHAT:
        selectedIP = displayNewChatScreen();
        if (selectedIP == "")
          break;
        _sendingRequest = std::async(std::launch::async, [portNum, selectedIP]() { establishConnection(selectedIP, portNum); });
        displayChatScreen(selectedIP);
        break;
      case CHATS: // TODO add mutex for connectedPeers
        selectedIP = displayChatsScreen(connectedPeers);
          
        if (selectedIP == "")
          break;
        displayChatScreen(selectedIP);
        break;
      case CHANGE_NICKNAME:
        nick = displayChangeNicknameScreen(globalNickname);
        if (nick == "~|NO_change|~")
          break;
        nickMutex.lock();
        if (nick != globalNickname) {
          globalNickname = nick;
          changeNickname = true;
        }
        nickMutex.unlock();
        break;
      case EXIT: // TODO end all processes
        showUI = false;
        break;
    }
  }
  endUI();
  
  return 0;
}
