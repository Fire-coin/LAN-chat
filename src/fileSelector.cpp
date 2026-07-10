#include "fileSelector.hpp"
#include "fileFuncs.hpp" // for file functions
#include "formatString.hpp"
/* Shows a file selector window, centered on the screen.
 * User can move with arrows or vim motions up and down. */
std::string displayFileSelector() {
  int startY = ROWS / 2 - ROWS / 4;
  int startX = COLS / 2 - COLS / 4;
  int opt;
  std::vector<std::string> files;
  std::string curPath = getCurDir();

  WINDOW* win;
  WINDOW* selectorWin;

  win = newwin(ROWS / 2, COLS / 2, startY, startX);
  selectorWin = newwin(ROWS / 2 - 4, COLS / 2 - 2, startY + 3, startX + 1);

  box(win, 0, 0);
  curs_set(0); // Invisible cursor
  
  mvwprintw(win, 1, 1, "%s", format("====File Selector====", COLS / 2 - 2, '^').c_str());

  wrefresh(win);
  
  while (1) {
    /* Print current path at the top, not wrapped */
    mvwprintw(win, 2, 1, "%s", format(curPath, COLS / 2 - 2, '<').c_str());
    wrefresh(win);
    
    getDirContents(files, curPath);

    opt = selector(files, selectorWin, ROWS / 2 - 4, COLS / 2 - 2);

    if (opt == -1) {
      wclear(win);
      wrefresh(win);
      return ""; // No file was selected
    }
    else {
      /* Go up a directory */
      if (files[opt] == "..") {
        curPath = getParentDir(curPath);
        continue;
      }

      /* A directory was selected, go into that directory */
      if (files[opt].back() == '/') { 
        curPath = changeDir(curPath, files[opt].substr(0, files[opt].size() - 1));
        continue;
      }

      /* A file was selected */
      wclear(win);
      wrefresh(win);
      return curPath + '/' + files[opt];
    }
  }
}
