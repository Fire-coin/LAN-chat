#include "UI.hpp"
#include "peer.hpp"
#include "formatString.hpp"
#include <ncurses.h>
#include <cstring> // memset
#include <algorithm>

int ROWS, COLS;

std::string inputBuffer = "";
bool updateScreen = false;
bool displayChat = true;
bool isInputReady = false;
IPToHistoryMap chatHistory;

std::vector<std::string> homeScreenOptions = {"New Chat", "Chats", "Change Nickname", "Exit"};
std::atomic<HOME_OPTIONS> selectedOption;


void addMsg(std::string IP, Msg msg, int creator) {
  std::pair<int, Msg> temp = std::pair<int, Msg>(creator, msg);
  chatHistory[IP].push_back(temp); //TODO maybe use emplace_back
  updateScreen = true;
}

void beginUI() {
  initscr();
  noecho();
  halfdelay(1);
  getmaxyx(stdscr, ROWS, COLS);
}

void endUI() {
  endwin();
}

// Inspired from https://invisible-island.net/ncurses/NCURSES-Programming-HOWTO.html#INIT 
void displayChatLog(WINDOW* win, std::string IP) {
  int x, y;
  wclear(win);
  // TODO add color to distinguish between peers
  for (auto p : chatHistory[IP]) {
    getyx(win, y, x);
    if (p.first == 0) {
      if (p.second.filename == "") 
        mvwprintw(win, y + 1, 0, "You have sent message: %s\n", p.second.data.c_str());
      else
        mvwprintw(win, y + 1, 0, "You have sent a file: %s\n", p.second.filename.c_str());
    }
    else {
      if (p.second.filename == "")
        mvwprintw(win, y + 1, 0, "Message recieved: %s\n", p.second.data.c_str());
      else
        mvwprintw(win, y + 1, 0, "File recieved: %s\n", p.second.filename.c_str());
    }
  }
  wrefresh(win); // To display it into real terminal
}

void displayChatScreen(std::string IP) {
  noecho();
  halfdelay(1); // Check for request every 200 milliseconds that user does not type
  WINDOW* chatWin;
  WINDOW* inputWin;
  int ch;


  chatWin = newwin(ROWS - 3, COLS, 0, 0);
  inputWin = newwin(3, COLS, ROWS - 3, 0);
  
  wclear(chatWin);
  wclear(inputWin);

  // Display a bar separating chat and input windows
  char* separator = new char[COLS];
  memset(separator, '-', COLS);
  mvwprintw(inputWin, 0, 0, "%s", separator);

  wrefresh(chatWin);
  wrefresh(inputWin);

  displayChat = true;
  updateScreen = true;
  wprintw(inputWin, "%s", inputBuffer.c_str());
  wrefresh(inputWin);

  while (displayChat) {
    if (updateScreen) {
      displayChatLog(chatWin, IP);
      updateScreen = false;
    }
    
    ch = wgetch(inputWin);
    if (ch == ERR)
      continue;

    // 127 is delete
    if (ch >= ' ' && ch <= 127 && !isInputReady) {
      if (ch != 127) {
        inputBuffer.append(reinterpret_cast<char*>(&ch));
        waddch(inputWin, ch);
        wrefresh(inputWin);
      } else {
        int x, y;
        getyx(inputWin, y, x);
        if (x == 0)
          continue;
        inputBuffer.erase(inputBuffer.size() - 1, 1); // Erase last character from buffer
        mvwdelch(inputWin, y, x - 1);
        wrefresh(inputWin);
      }
    }
    
    // Enter has been pressed
    if (ch == 10) {
      isInputReady = true;
      // TODO make better way to clear the line
      wclear(inputWin);
      mvwprintw(inputWin, 0, 0, "%s", separator);
      wrefresh(inputWin);
    }
    if (ch == 27) { // Escape
      clear();
      refresh();
      displayChat = false;
    }
  }
  
  delete[] separator;
  endwin();
}

/* Displays option menu and returns the index of selected option */
int selector(std::vector<std::string>& options, WINDOW* win) {
  noecho();
  cbreak(); // Immediately get key press

  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible

  int current = 0;
  for (int i = 0; i < options.size(); ++i) {
    if (i == current) {
      wprintw(win, "[*] %s\n", options[i].c_str());
    } else
      wprintw(win, "[ ] %s\n", options[i].c_str());
  }
  wrefresh(win);
  int ch;
  bool update = false;
  while (1) {
    ch = wgetch(win);
    
    update = true;
    switch (ch) {
      case 'k':
      case KEY_UP:
        if (current == 0)
          current = options.size() - 1;
        else
          current--;
        break;
      case 'j':
      case KEY_DOWN:
        if (current == options.size() - 1)
          current = 0;
        else
          current++;
        break;
      case 10: // Enter key
        wclear(win);
        wrefresh(win);
        return current;
      case 27: // Escape
        wclear(win);
        wrefresh(win);
        return -1;
      default:
        update = false;
    }
    
    if (update) {
      for (int i = 0; i < options.size(); ++i) {
        mvwprintw(win, i, 1, " ");
        if (i == current)
          mvwprintw(win, i, 1, "*");
      }
      wrefresh(win);
    }
  }
}

HOME_OPTIONS displayHomeScreen() {
  // TODO make and display logo
  clear();
  int x, y;
  
  getyx(stdscr, y, x);
  // TODO use map of WINDOW* and std::string to store windows and just refresh them
  WINDOW* homeWin;
  WINDOW* optionWin;
  
  homeWin = newwin(ROWS, COLS, 0, 0);
  wprintw(homeWin, "====LAN-chat====\n");

  optionWin = newwin(homeScreenOptions.size(), COLS, y + 1, 0);
  
  refresh();
  wrefresh(homeWin);
  int userChoice = selector(homeScreenOptions, optionWin);

  switch (userChoice) {
    case -1:
      return NO_OPTION;
    case 0: // New chat
      return NEW_CHAT;
    case 1: // Chats
      return CHATS;
    case 2: // Change nickname
      return CHANGE_NICKNAME;
    case 3: // Exit
      return EXIT;
    default: // How tf u managed to corrupt memory so bad to get new index?
      return NO_OPTION;
  }
}

std::string displayNewChatScreen() {
  clear();
  WINDOW* win;
  WINDOW* optionWin;
  win = newwin(ROWS, COLS, 0, 0);
  optionWin = newwin(ROWS - 2, COLS, 2, 0);

  // TODO make better header
  wprintw(win, "====SELECT PEER TO CONNECT====\n");
  wprintw(win, "        IP         |    nickname\n");
  
  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible
  halfdelay(1);

  discoverMutex.lock();
  int lastSize = currentPeers.size();


  int current = 0;
  for (int i = 0; i < currentPeers.size(); ++i) {
    // Not showing IP with which the connection is already established
    // TODO implement a mutex
    if (std::find_if(connectedPeers.begin(), connectedPeers.end(), [i](Peer p) {return p.IP == currentPeers[i].IP;}) != connectedPeers.end())
        continue;
    if (i == current) {
      wprintw(optionWin, "[*] %s|%s\n", format(currentPeers[i].IP, 3 * 4 + 3, '<').c_str(), currentPeers[i].nickname.c_str());
    } else
      wprintw(optionWin, "[ ] %s|%s\n", format(currentPeers[i].IP, 3 * 4 + 3, '<').c_str(), currentPeers[i].nickname.c_str());
  }
  discoverMutex.unlock();
  wrefresh(win);
  int ch;
  bool update = false;
  while (1) {
    ch = wgetch(optionWin);
    if (ch == 27) // Escape
      return "";
    if (ch == 10) {
      discoverMutex.lock();
      std::string out = currentPeers.size() > 0 ? currentPeers[current].IP : "";
      discoverMutex.unlock();

      wclear(optionWin);
      wrefresh(optionWin);
      return out;
    }
    update = true;
    switch (ch) {
      case 'k':
      case KEY_UP:
        if (current == 0)
          current = currentPeers.size() > 0 ? currentPeers.size() - 1 : 0;
        else
          current--;
        break;
      case 'j':
      case KEY_DOWN:
        if (currentPeers.size() < 1)
          current = 0;
        if (current == currentPeers.size() - 1)
          current = 0;
        else
          current++;
        break;
      default:
        update = false;
    }
    
    if (update) {
      for (int i = 0; i < currentPeers.size(); ++i) {
        mvwprintw(optionWin, i, 1, " ");
        if (i == current)
          mvwprintw(optionWin, i, 1, "*");
      }
      wrefresh(optionWin);
    }
    discoverMutex.lock();
    if (currentPeers.size() != lastSize) {
      if (lastSize > currentPeers.size()) // If list got smaller and selected was last option, place selected at the new last option
        current = currentPeers.size() - 1;
      lastSize = currentPeers.size();
      wclear(optionWin);
      wmove(optionWin, 0, 0);
      
      for (int i = 0; i < currentPeers.size(); ++i) {
        if (i == current) {
          wprintw(optionWin, "[*] %s|%s\n", currentPeers[i].IP.c_str(), currentPeers[i].nickname.c_str());
        } else
          wprintw(optionWin, "[ ] %s|%s\n", currentPeers[i].IP.c_str(), currentPeers[i].nickname.c_str());
      }
      wrefresh(optionWin);
    }
    discoverMutex.unlock();
  }
}

bool displayConfirmWin(std::string question) {
  WINDOW* win;
  int height = 8, width = COLS / 3;
  int startX, startY;
  startY = (ROWS - height) / 2;
  startX = (COLS - width) / 2;
  win = newwin(height, width, startY, startX);
  box(win, 0, 0);

  mvwprintw(win, 1, 1, "%s", format(question, width - 2, '^').c_str());

  WINDOW* yes;
  WINDOW* no;
  yes = newwin(3, 5, startY + height - 4, startX + 1);
  no = newwin(3, 4, startY + height - 4, startX + width - 5);

  box(yes, 0, 0);
  box(no, 0, 0);

  wattron(yes, A_REVERSE);
  mvwprintw(yes, 1, 1, "Yes");
  wattroff(yes, A_REVERSE);
  mvwprintw(no, 1, 1, "No");

  wrefresh(win);
  wrefresh(yes);
  wrefresh(no);
  refresh();

  int ch;
  halfdelay(1);
  keypad(win, true);
  
  bool current = true; // For yes option

  
  while (1) {
    ch = wgetch(win);

    switch (ch) {
      case 'l':
      case 'h':
      case KEY_RIGHT:
      case KEY_LEFT:
        if (current) {
          mvwprintw(yes, 1, 1, "Yes");
          wattron(no, A_REVERSE);
          mvwprintw(no, 1, 1, "No");
          wattroff(no, A_REVERSE);
        } else {
          mvwprintw(no, 1, 1, "No");
          wattron(yes, A_REVERSE);
          mvwprintw(yes, 1, 1, "Yes");
          wattroff(yes, A_REVERSE);
        }
        wrefresh(no);
        wrefresh(yes);

        current = !current;
        break;
      case '\n':
        return current;
    }
  }
}

std::string displayChangeNicknameScreen(std::string curNick) {
  clear();
  WINDOW* textWin;
  WINDOW* nickWin;

  // Center nickname window
  int length = 32;
  int avgX, avgY, startX, startY;
  avgX = COLS / 2;
  
  startX = avgX - (length + 1) / 2; // 1 for border
  
  nickWin = newwin(3, length + 2, 1, startX);
  box(nickWin, 0, 0);
  
  textWin = newwin(1, length + 2, 0, startX);
  wprintw(textWin, "Your nickname\n");

  // Putting current nickname into input window
  mvwprintw(nickWin, 1, 1, "%s", curNick.c_str());

  refresh();
  wrefresh(nickWin);
  wrefresh(textWin);

  halfdelay(1);
  int ch;
  while (1) {
    ch = wgetch(nickWin);
    
    if (ch == ERR)
      continue;

    // 127 is delete
    if (ch >= ' ' && ch <= 127) {
      int x, y;
      getyx(nickWin, y, x);
      if (ch != 127 && x < length) {
        curNick.append(reinterpret_cast<char*>(&ch));
        waddch(nickWin, ch);
        wrefresh(nickWin);
      } else {
        if (x == 1)
          continue;
        curNick.erase(curNick.size() - 1, 1); // Erase last character from buffer
        wmove(nickWin, y, x - 1);
        waddch(nickWin, ' ');
        wmove(nickWin, y, x - 1);
        wrefresh(nickWin);
      }
    }

    if (ch == 10)
      return curNick;
    if (ch == 27) { // Escape
      bool responce = displayConfirmWin("Do you want to save changes?");
      if (responce)
        return curNick;
      else
        return "~|NO_change|~";
    }
  }
}

//TODO fix the lateness after pressing Escape
std::string displayChatsScreen(std::vector<Peer>& connectedPeers) {
  clear();
  std::vector<std::string> options;
  
  std::string entry;
  for (auto p : connectedPeers) {
    entry.clear();
    entry.append(p.nickname);
    entry.append(" | ");
    entry.append(p.IP);
    options.push_back(entry);
  }

  WINDOW* win;
  WINDOW* selectorWin;

  win = newwin(ROWS, COLS, 0, 0);

  wprintw(win, "Select a chat to enter\n");
  wprintw(win, "nickname   | IP\n");
  
  wrefresh(win);
  selectorWin = newwin(options.size(), COLS, 2, 0);
  if (options.size() == 0) {
    wprintw(selectorWin, "No peers connected\n");
    wrefresh(selectorWin);
  }
  int opt = selector(options, selectorWin);

  if (opt == -1)
    return "";

  return connectedPeers[opt].IP;
}
