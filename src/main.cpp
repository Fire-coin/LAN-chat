#include "socket.hpp"
#include "UI.hpp"
#include "socketFuncs.hpp"
#include "peer.hpp"
#include <vector> // std::vector
#include <string> // std::string
#include <thread> // std::thread


int main() {
  
  int choice;
  int discoveryPortNum = 5000;
  std::string selectedIP;
  std::string nick;
  HOME_OPTIONS selectedOption = NO_OPTION;
  
  /* Trying to start up handling peer requests. If it succeeds, then
   * connection status is set to 1. At the same time it changes 
   * discoveryPortNum, such that it is same one as monitoring socket is
   * listening on. */
  std::thread peerRequestsThread(handlePeerRequestsWrapper);
  while (connectionStatus == 0) {}
  if (connectionStatus == -1) {
    return -1;
  }
  
  /* Starting peer discovery, sending and recieving packets. */
  std::thread discoveryThread(discoverPeers, appPortNum.load(), discoveryPortNum);
  
  /* Strting up UI with ncurses */
  beginUI();
  /* Main app loop */
  while (showUI) {
    selectedOption = displayHomeScreen();

    switch (selectedOption) {
      case NEW_CHAT:
        selectedIP = displayNewChatScreen();
        if (selectedIP == "")
          break;
        establishConnection(selectedIP);
        displayChatScreen(selectedIP);
        break;
      case CHATS:
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
      case EXIT:
        handleRequests = false;
        doPeerDiscovery = false;
        showUI = false;
        endRequestHandling();
        break;
      default:
        break;
    }
  }
  endUI();

  peerRequestsThread.join();
  discoveryThread.join();
  
  return 0;
}
