/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/
// we store the original terminal attributes in a global variable, orig_termios.
// we assign the orig_termios struct to the raw struct in order to make copy of it before
// we start making our changes.
struct termios orig_termios;

/*** terminal ***/
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}
// perror() comes from <stdio.h>, and exit() comes from <stdlib.h>.

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}

// Turning off echoing
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");

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

  // ICRNL comes from <termios.h>. The I stands for “input flag”, CR stands for
  // "carriage return", and NL stands for "new line". (basically we are fixing Ctrl-M here)
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  // by default, Ctrl-S and Ctrl-Q are used for software flow control. Ctrl-S stops data from being transmitted
  // to the terminal until you press Ctrl-Q.
  // fun fact: this originates in the days when you might want to pause the transmission of data to let a device like printer catch up.
  // IXON comes from <termios.h>. The I stands for “input flag” and XON comes from teh n and ames of the two control characters that Ctrl-S
  // Ctrl-Q produce: XOFF to pause transmission and XON to resume transmission.

  // turn off all output processing features by turning off the OPOST flag.
  raw.c_oflag &= ~(OPOST);

  // BRKINT, INPCK, ISTRIP, and CS8 all come from <termios.h>.
  // When BRKINT is turned on, a break condition will cause a SIGINT signal to be sent to the program, like pressing Ctrl-C.
  // INPCK enables parity checking, ISTRIP causes the 8th bit of each input byte to be stripped, meaning it will set it to 0.
  // CS8 is not a flag, it is a bit mask with multiple bits, which we set using the bitwise-OR (|) operator unlike all the flags
  // we are turning off. It sets the character size (CS) to 8 bits per byte.
  raw.c_cflag |= (CS8);

  // added IEXTEN flag to disable Ctrl-V
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  // ECHO is a bitflag, defined as 00000000000000000000000000001000 in binary.
  // we use the bitwise-NOT operator (~) on this value to get 11111111111111111111111111110111.
  // we then bitwise-AND this value with the flags field, which forces the fourth bit in the flags
  // to become 0, and causes every other bit to retain its current value.

  // ICANON comes from <termios.h>. Input flags (the ones in the c_iflag field) generally start with
  // I like ICANON does. However, ICANON is not an input flag, it’s a “local” flag in the c_lflag field.

  // By default, Ctrl-C sends a SIGINT signal to the current process which causes it to
  // terminate, and Ctrl-Z sends a SIGTSTP signal to the current process which causes it to suspend.
  // ISIG comes from <termios.h>. Like ICANON, it starts with I but isn’t an input flag.

  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  // VMIN and VTIME come from <termios.h>. They are indexes into the c_cc field, which stands for “control characters”,
  // an array of bytes that control various terminal settings.
  // VMIN value sets the minimum number of bytes of input needed before read() can return().
  // we set it to 0 so that read() returns as soon as there is any input to be read.
  // VTIME value sets the maximum amount of time to wait before read() returns.
  // it is in tenths of a second, so we set it to 1/10 of second, or 100ms.
  // if read() times out, it will return 0, which makes sense because its usual return value is the number of bytes read.

  // after the attributes have been modified, you can then apply them to the terminal using tcsetattr().
  // the TCSAFLUSH argument specifies when to apply the change: in this case, it waits for all pending
  // output to be written to the terminal, and also discards any input that hasn’t been read.
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if(nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}
// editorReadKey()'s job is to wait for one keypress, and return it.

/*** output ***/
void editorRefreshScreen(){
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
}
// write() and STDOUT_FILENO come from <unistd.h>.
// the 4 in our write() call means we are writing 4 bytes out to the terminal.
// the first byte it \x1b, which is the escape character, or 27 in decimal.
// the other 3 bytes are [2J

// second escape sequence is only 3 bytes long, and uses the H command (cursor position) to
// position the cursor. the H command actually takes two arguments: the row number and the column number
// at which to position the cursor.

// extra info:
// we are writing an escape sequence to the terminal. escape sequences always
// starts with an escape character (27) followed by a [ character. escape sequences instruct the
// terminal to do various text formatting tasks, such as coloring text, moving the cursor around, and
// clearing parts of th screen.
//
// we are using the J command (erase in display) to clear the screen. escape sequence commands take args
// which come before the command. in this case the arg is 2, which says to clear the entire screen. <esc>[1J would
// clear the screen from the cursor up to the end of the screen. also 0 is the default arg for J, so just <esc>[J by
// itself would also clear the screen from the cursor to the end.
//
// for this text editor we will mostly be using VT100 escape sequences, which are supported very widely by
// modern terminal emulators.

/*** input ***/
void editorProcessKeypress() {
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  }
}
// editorProcessKeypress() waits for a keypress, then handles it.

/*** init ***/
int main(){
  enableRawMode();

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
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  // added a timeout for read(), so that read() returns if it doesn’t get any input for a certain amount of time.

  // errno and EAGAIN come from <errno.h>.
  // tcsetattr(), tcgetattr(), and read() all return -1 on failure, and set the errno value to indicate the error.

  // printf() can print multiple representations of a byte. %d tells it to format the byte
  // as a decimal number (it's ASCII code), and %c tells it to write out the byte directly, as a character.
  return 0;
}
