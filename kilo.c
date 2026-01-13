#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// we store the original terminal attributes in a global variable, orig_termios.
// we assign the orig_termios struct to the raw struct in order to make copy of it before
// we start making our changes.
struct termios orig_termios;

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// Turning off echoing
void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);

  // atexit() comes from <stdlib.h>.
  // we use it to register our disableRawMode() function to be called automatically
  // when the program exits, whether it exits by return from main(), or by calling the exit() function.
  // this ensures, we'll leave the terminal attributes the way we found them when our program exits.
  atexit(disableRawMode);

  // We can set a terminal's attributes by
  // 1) using tcgetattr() to read the current attributes into a struct
  // 2) modifying the struct by hand and
  // 3) passing the modified struct to tcsetattr() to write the new terminal attributes back out.

  // struct termios, tcgetattr(), tcsetattr(), ECHO, and TCSAFLUSH all come from <termios.h>.

  struct termios raw = orig_termios;

  // Terminal attributes can be read into a termios struct by tcgetattr().
  // the c_lflag field is for "local flags". the other flags fields are
  // c_iflag (input flags), c_oflag (output flags), and c_cflag (control flags),
  // all of which we will have to modify to enable raw mode
  // turning off canonical mode
  raw.c_lflag &= ~(ECHO | ICANON);
  // ECHO is a bitflag, defined as 00000000000000000000000000001000 in binary.
  // we use the bitwise-NOT operator (~) on this value to get 11111111111111111111111111110111.
  // we then bitwise-AND this value with the flags field, which forces the fourth bit in the flags
  // to become 0, and causes every other bit to retain its current value.

  // ICANON comes from <termios.h>. Input flags (the ones in the c_iflag field) generally start with
  // I like ICANON does. However, ICANON is not an input flag, it’s a “local” flag in the c_lflag field.

  // after the attributes have been modified, you can then apply them to the terminal using tcsetattr().
  // the TCSAFLUSH argument specifies when to apply the change: in this case, it waits for all pending
  // output to be written to the terminal, and also discards any input that hasn’t been read.
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(){
  enableRawMode();

  char c;
  // read() and STDIN_FILENO come from <unistd.h>.
  // we are asking read() to read 1 byte from standard input into the variable c
  // and to keep it doing until there are no more bytes to read.
  // read() returns the numbers of bytes that it read, and will return 0 when it
  // reaches the end of a file

  // iscntrl() comes from <ctype.h>, and printf() comes from <stdio.h>.
  // iscntrl() tests whether a character is a control character. Control characters are
  // nonprintable characters that we don't want to print to the screen.
  // ASCII codes 0-31 are all control characters, and 127 is also a control character.
  // ASCII codes 32-216 are all printable.
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
    if (iscntrl(c)) {
      printf("%d\n", c);
    } else {
      printf("%d ('%c')\n", c, c);
    }
  }

  // printf() can print multiple representations of a byte. %d tells it to format the byte
  // as a decimal number (it's ASCII code), and %c tells it to write out the byte directly, as a character


  return 0;
}
