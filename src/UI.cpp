#include "UI.hpp"
#include "peer.hpp"
#include "formatString.hpp"
#include "fileSelector.hpp"
#include <ncurses.h>
#include <cstring> // memset
#include <algorithm>
#include <cassert>
#include "socketFuncs.hpp"

int ROWS, COLS;

std::string inputBuffer = "";
bool updateScreen = false;
bool displayChat = true;
bool isInputReady = false;
IPToHistoryMap chatHistory;

std::mutex notificationMutex;
std::unordered_map<std::string, int> notifications;
int totalNotifications = 0;

std::vector<std::string> homeScreenOptions = {"New Chat", "Chats", "Change Nickname", "Exit"};
std::atomic<HOME_OPTIONS> selectedOption;

void displayError(std::string error) {
  WINDOW* win;
  curs_set(0);
  halfdelay(5);
  // Center the window
  int startY = ROWS / 4;
  int startX = COLS / 4;
  win = newwin(ROWS / 2, COLS / 2, startY, startX);
  box(win, 0, 0);
  mvwprintw(win, 1, 1, "%s", format("====LAN-chat Error====", COLS / 2 - 2, '^').c_str());
  mvwprintw(win, 2, 1, "%s", format(error, COLS / 2 - 2, '^').c_str());
  mvwprintw(win, ROWS / 2 - 2, 1, "%s", format("Press Escape to close this window", COLS / 2 - 2, '^').c_str());
  wrefresh(win);
  int ch = ERR;
  while (ch != 27) {
    ch = wgetch(win);
  }
}

void addMsg(std::string IP, Msg msg, int creator) {
  if (creator == 1) { // A message recieved
    notificationMutex.lock();
    notifications[IP]++;
    totalNotifications++;
    notificationMutex.unlock();
  }

  //std::pair<int, Msg> temp = std::pair<int, Msg>(creator, msg);
  chatHistory[IP].emplace_back(creator, msg); //TODO maybe use emplace_back
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
  notificationMutex.lock();
  totalNotifications -= notifications[IP];
  notifications[IP] = 0;
  notificationMutex.unlock();
  wrefresh(win); // To display it into real terminal
}
// TODO make the messages scrollable
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
      } else {
        int x, y;
        getyx(inputWin, y, x);
        if (x == 0)
          continue;
        inputBuffer.erase(inputBuffer.size() - 1, 1); // Erase last character from buffer
        mvwdelch(inputWin, y, x - 1);
      }
      int index = inputBuffer.find(":file");
      if (index != inputBuffer.npos) {
        inputBuffer.erase(index, 5); // Erase the :file
        std::string filename = displayFileSelector();
        if (filename != "") {
          inputBuffer = "$file=" + filename + inputBuffer;
          mvwprintw(inputWin, 1, 0, "%s", format(inputBuffer, COLS, '<').c_str());
          wmove(inputWin, 1, inputBuffer.size());
        }
      }
      wrefresh(inputWin);
    }
    
    // Enter has been pressed
    if (ch == 10) {
      sendMessage(IP, inputBuffer);
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
int selector(std::vector<std::string>& options, WINDOW* win, int rows, int cols) {
  noecho();
  cbreak();

  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible

  int current = 0;
  for (int i = 0; i < rows; ++i) {
    if (i >= options.size()) {
      wprintw(win, "%s\n", format("", cols - 1, '<').c_str());
      continue;
    }
    if (i == current) {
      wprintw(win, "[*] %s\n", options[i].c_str());
    } else
      wprintw(win, "[ ] %s\n", options[i].c_str());
  }
  wrefresh(win);
  int ch;
  bool update = false;
  int arrayBegin = 0;
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
      if (arrayBegin != current / rows) { // It does not fit into single window, scroll down
        arrayBegin = (current / rows) * rows; // should work, lazy to prove why
        for (int i = 0; i < rows; ++i) {
          if (i + arrayBegin < options.size())
            mvwprintw(win, i, 0, "[ ] %s", format(options[i + arrayBegin], cols, '<').c_str());
          else
            mvwprintw(win, i, 0, "%s", format("", cols, '^').c_str());
        }
      }
      for (int i = 0; i < rows; ++i) {
        mvwprintw(win, i, 1, " ");
        if (i == (current % rows))
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
  auto hso = homeScreenOptions;
  notificationMutex.lock();
  if (totalNotifications > 0) {
    hso[1] += " {";
    hso[1] += std::to_string(totalNotifications);
    hso[1] += '}';
  }
  notificationMutex.unlock();

  
  int userChoice = selector(hso, optionWin, homeScreenOptions.size(), COLS);

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
    if (std::find_if(connectedPeers.begin(), connectedPeers.end(), [i](Peer* p) {return p->IP == currentPeers[i].IP;}) != connectedPeers.end())
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

// TODO make it possible to end chats with some key e.g backspace or q
// TODO fix bug, where when in Chats screnn on one device, when trying to connect to
// other device, the other device gets infite empty messages. Only sometimes
std::string displayChatsScreen(std::vector<Peer*>& connectedPeers) {
  clear();
  std::vector<std::string> options;
  std::vector<std::string> tempOptions;
  
  std::string entry;

  WINDOW* win;
  WINDOW* selectorWin;

  win = newwin(ROWS, COLS, 0, 0);

  wprintw(win, "Select a chat to enter\n");
  wprintw(win, "nickname   | IP\n");
  wrefresh(win);
  int rows = ROWS - 2;
  int cols = COLS;
  selectorWin = newwin(rows, cols, 2, 0);

  int ch;
  bool update = false;
  int arrayBegin = 0;
  bool updateWhole = true;
  int current = 0;
  
  noecho();
  halfdelay(10); // Check for update every second

  keypad(win, true);
  curs_set(0); // Sets cursor to be invisible
  while (1) {
    tempOptions.clear();
    discoverMutex.lock();
    for (int i = 0; i < connectedPeers.size(); ++i) {
      entry.clear();
      entry += format(connectedPeers[i]->IP, 3 * 4 + 3, '<');
      entry += '|';
      entry += connectedPeers[i]->nickname;
      notificationMutex.lock();
      if (notifications[connectedPeers[i]->IP] > 0) {
        entry += " {";
        entry += std::to_string(notifications[connectedPeers[i]->IP]);
        entry += "}";
      }
      notificationMutex.unlock();
      tempOptions.push_back(entry);
    }
    discoverMutex.unlock();

    // Check if new options are different
    if (options.size() != tempOptions.size()) {
      updateWhole = true;
      options = tempOptions;
    }
    else {
      for (int i = 0; i < options.size(); ++i) {
        if (options[i] != tempOptions[i]) {
          updateWhole = true;
          options = tempOptions;
          break;
        }
      }
    }

    if (updateWhole) {
      if (options.size() == 0) {
        wclear(selectorWin);
        mvwprintw(selectorWin, 0, 0, "No Peers Connected\n");
        wrefresh(selectorWin);
        updateWhole = false;
        continue;
      }
      for (int i = 0; i < rows; ++i) {
        if (i >= options.size()) {
          mvwprintw(selectorWin, i, 0, "%s\n", format("", cols - 1, '<').c_str());
          continue;
        }
        if (i == current) {
          mvwprintw(selectorWin, i, 0, "[*] %s\n", options[i].c_str());
        } else {
          mvwprintw(selectorWin, i, 0, "[ ] %s\n", options[i].c_str());
        }
      }
      wrefresh(selectorWin);
      updateWhole = false;
    }

    ch = wgetch(selectorWin);
    
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
        wclear(selectorWin);
        wrefresh(selectorWin);
        return connectedPeers[current]->IP;
      case 27: // Escape
        wclear(selectorWin);
        wrefresh(selectorWin);
        return "";
      default:
        update = false;
    }
    if (update) {
      if (arrayBegin != current / rows) { // It does not fit into single window, scroll down
        arrayBegin = (current / rows) * rows; // should work, lazy to prove why
        for (int i = 0; i < rows; ++i) {
          if (i + arrayBegin < options.size()) {
            mvwprintw(selectorWin, i, 0, "[ ] %s", format(options[i + arrayBegin], cols, '<').c_str());
          }
          else
            mvwprintw(selectorWin, i, 0, "%s", format("", cols, '^').c_str());
        }
      }
      for (int i = 0; i < rows; ++i) {
        mvwprintw(selectorWin, i, 1, " ");
        if (i == (current % rows))
          mvwprintw(selectorWin, i, 1, "*");
      }
      wrefresh(selectorWin);
    }
  }
}
