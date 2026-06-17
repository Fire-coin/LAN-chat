#include "fileSelector.hpp"
#include "fileFuncs.hpp" // for file functions
#include "formatString.hpp"

std::string displayFileSelector() {
  WINDOW* win;
  int startY = ROWS / 2 - ROWS / 4;
  int startX = COLS / 2 - COLS / 4;
  win = newwin(ROWS / 2, COLS / 2, startY, startX);
  box(win, 0, 0);
  curs_set(0); // Invisible cursor
  
  mvwprintw(win, 1, 1, "%s", format("====File Selector====", COLS / 2 - 2, '^').c_str());

  wrefresh(win);
  
  WINDOW* selectorWin;
  selectorWin = newwin(ROWS / 2 - 4, COLS / 2 - 2, startY + 3, startX + 1);

  std::vector<std::string> files;
  std::string curPath = getCurDir();
  int opt;
  

  while (1) {
    mvwprintw(win, 2, 1, "%s", format(curPath, COLS / 2 - 2, '<').c_str());
    wrefresh(win);
    
    getDirContents(files, curPath);

    opt = selector(files, selectorWin, ROWS / 2 - 4, COLS / 2 - 2);

    if (opt == -1)
      return ""; // No file was selected
    else {
      if (files[opt] == "..") {
        curPath = getParentDir(curPath);
        continue;
      }
      if (files[opt].back() == '/') { // go into other directory
        curPath = changeDir(curPath, files[opt].substr(0, files[opt].size() - 1));
        continue;
      }
      wclear(win);
      wrefresh(win);
      return files[opt];
    }
  }
}
