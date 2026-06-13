#include <stdlib.h>

int main() {
  system("g++ -o main main.cpp socket.cpp UI.cpp socketFuncs.cpp fileFuncs.cpp peer.cpp -lcurses");


  return 0;
}
