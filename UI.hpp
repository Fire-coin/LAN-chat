#ifndef UI_HPP
#define UI_HPP

#include "socket.hpp" // For Msg struct
#include <vector>
#include <utility> // std::pair
#include <string>

std::string inputBuffer;
bool acceptInputToBuffer;

void displayChatLog(std::vector<std:pair<int, Msg>>& chatLog);
std::string getUserInput();



#endif

#pragma once
