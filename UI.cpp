#include "UI.hpp"
#include <curses.h>
#include <stdlib.h>


// Inspired from https://invisible-island.net/ncurses/NCURSES-Programming-HOWTO.html#INIT 
void displayChatLog(std::vector<std:pair<int, Msg>>& chatLog) {
  system("clear");
  initsrc(); // Start curses mode
  noecho();

 // TODO add color to distinguish between peers
  for (auto p : chatLog) {
    if (p.first == 0) {
      if (p.second.filename == "") 
        printw("You have sent message: %s\n", p.second.data);
      else
        printw("You have sent a file: %s\n", p.second.filename);
    }
    else {
      if (p.second.filename == "")
        printw("Message recieved: %s\n", p.second.data);
      else
        printw("File recieved: %s\n", p.second.filename);
    }
    printw("\n");
  }
  
  printw("\nInput buffer (type $file=your_filename to send a file): %s", inputBuffer.c_str());

  refresh(); // To display it into real terminal
  endwin(); // End curses mode
}

acceptInputToBuffer = true;

std::string getUserInput() {
  inputBuffer = "";
  initsrc(); // Start curses mode
  raw();
  keypad(stdsrc, TRUE);
  noecho();
  // TODO handle CTRL+C case later
  int ch;
  do {
    while (acceptInputToBuffer) {
      ch = getch();
      if (ch >= ' ' && ch <= '~')
        inputBuffer.append((char)ch);
    }
  } while (ch != KEY_ENTER);

  endwin();
  return inputBuffer;
}
