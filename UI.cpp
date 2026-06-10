#include "UI.hpp"
#include <curses.h>
#include <stdlib.h>

bool acceptInputToBuffer = true;
std::string inputBuffer = "";

// Inspired from https://invisible-island.net/ncurses/NCURSES-Programming-HOWTO.html#INIT 
void displayChatLog(std::vector<std::pair<int, Msg>>& chatLog) {
  int row, col, x, y;
  initscr(); // Start curses mode
  clear();
  noecho();

  getmaxyx(stdscr, row, col);

 // TODO add color to distinguish between peers
  for (auto p : chatLog) {
    getyx(stdscr, y, x);
    if (p.first == 0) {
      if (p.second.filename == "") 
        mvprintw(y + 1, 0, "You have sent message: %s\n", p.second.data);
      else
        mvprintw(y + 1, 0, "You have sent a file: %s\n", p.second.filename);
    }
    else {
      if (p.second.filename == "")
        mvprintw(y + 1, 0, "Message recieved: %s\n", p.second.data);
      else
        mvprintw(y + 1, 0, "File recieved: %s\n", p.second.filename);
    }
  }
  
  mvprintw(row, 0, "\nInput buffer (type $file=your_filename to send a file): %s", inputBuffer.c_str());

  refresh(); // To display it into real terminal
  endwin(); // End curses mode
}


std::string getUserInput() {
  inputBuffer = "";
  
  cbreak();
  keypad(stdscr, TRUE);
  noecho();
  // TODO handle CTRL+C case later
  int ch;
  do {
    while (acceptInputToBuffer) {
      ch = getch();
      if (ch >= ' ' && ch <= '~')
        inputBuffer.append(reinterpret_cast<char*>(&ch));
    }
  } while (ch != 10);

  return inputBuffer;
}

void testInputAndOutput() {
  int ch;
  initscr();
  clear();
  noecho();
  cbreak();
  
  while (1) {
    ch = getch();
    if (ch == 10) {
      printw(inputBuffer.c_str());
      inputBuffer = "";
    }
    else
      inputBuffer.append(reinterpret_cast<char*>(&ch));
  }

  endwin();
}
