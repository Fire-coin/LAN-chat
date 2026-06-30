#ifndef UI_HPP
#define UI_HPP

#include "socket.hpp" // For Msg struct
#include "peer.hpp" // For Peer struct
#include <vector> // std::vector
#include <utility> // std::pair
#include <string> // std::string
#include <curses.h> // for WINDOW
#include <atomic> // for std::atomic
#include <unordered_map>
#include <mutex>

enum HOME_OPTIONS {NEW_CHAT, CHATS, CHANGE_NICKNAME, EXIT, NO_OPTION};

extern int ROWS, COLS;

extern std::string inputBuffer;
extern std::atomic<bool> updateScreen;
extern bool displayChat;
extern std::atomic<bool> showUI;
// The message which will be shown in chat (no need to store file contents if it is sent)
struct ChatMsg {
  bool isFile;
  std::string data; // It is filename if isFile field is true, and message otherwise
  int creator;
};

// A map that maps IP of peer to the chat history with that peer
using IPToHistoryMap = std::unordered_map<std::string, std::vector<ChatMsg>>;
extern IPToHistoryMap chatHistory;
extern std::mutex chatHistoryMutex;

extern std::vector<std::string> homeScreenOptions;

void displayError(std::string error);
int checkError();

extern std::mutex notificationMutex;
extern std::unordered_map<std::string, int> notifications; // How many messages (int) recieved from other peer IP (std::string)
extern int totalNotifications;
void addMsg(std::string IP, Msg msg, int creator);

void beginUI();
void endUI();

void displayChatLog(WINDOW* win, std::string IP);
void displayChatScreen(std::string IP);
int selector(std::vector<std::string>& options, WINDOW* win, int rows, int cols);
HOME_OPTIONS displayHomeScreen();
std::string displayNewChatScreen();
std::string displayChangeNicknameScreen(std::string curNick);
std::string displayChatsScreen(std::vector<std::shared_ptr<Peer>>& connectedPeers);




#endif

#pragma once
