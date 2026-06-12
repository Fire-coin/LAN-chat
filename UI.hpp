#ifndef UI_HPP
#define UI_HPP

#include "socket.hpp" // For Msg struct
#include <vector>
#include <utility> // std::pair
#include <string>


extern std::string inputBuffer;
extern bool updateScreen;
extern bool displayChat;
extern bool isInputReady;
extern std::vector<std::pair<int, Msg>> chatHistory;

void addMsg(Msg msg, int creator);

void displayChatLog(std::vector<std::pair<int, Msg>>& chatLog);
std::string getUserInput();
void displayChatScreen();
int selector(std::vector<std::string>& options);




#endif

#pragma once
