/*** includes ***/

/*
 * Standard Library Headers:
 * - <errno.h>      : Error number definitions (errno, EAGAIN)
 * - <stdio.h>      : Standard I/O (perror, printf, sscanf)
 * - <stdlib.h>     : General utilities (exit, atexit)
 * - <sys/ioctl.h>  : I/O control operations (ioctl, TIOCGWINSZ, struct winsize)
 * - <termios.h>    : Terminal I/O (termios, tcgetattr, tcsetattr, terminal
 * flags)
 * - <unistd.h>     : POSIX API (read, write, STDIN_FILENO, STDOUT_FILENO)
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"

/*
 * Macro: CTRL_KEY(k)
 * Converts a key to its Ctrl-key equivalent by masking with 0x1f (00011111).
 * This mirrors what the terminal does when Ctrl is pressed - it sets the upper
 * 3 bits of the character to 0.
 */
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  PAGE_UP,
  PAGE_DOWN
};

/*** data ***/

/*
 * Global Editor Configuration
 *
 * The editorConfig struct holds all global state for the editor:
 * - screenrows/screencols: Current terminal dimensions
 * - orig_termios: Original terminal attributes (saved for restoration on exit)
 *
 * We maintain the original terminal attributes so we can restore them when
 * the program exits, leaving the terminal in the state we found it.
 */
struct editorConfig {
  int cx, cy;
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

/*
 * Terminal Control Functions
 *
 * This section handles low-level terminal operations including:
 * - Error handling and cleanup
 * - Raw mode enable/disable
 * - Key reading
 * - Window size detection
 */

/*
 * die() - Error handling with cleanup
 *
 * Clears the screen, positions cursor, prints error message, and exits.
 * Uses perror() to print the error message with system error details.
 */
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

/*
 * disableRawMode() - Restore original terminal attributes
 *
 * Called automatically on program exit via atexit() to ensure the terminal
 * is left in the state we found it. Uses TCSAFLUSH to apply changes after
 * output is drained and input is discarded.
 */
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

/*
 * enableRawMode() - Configure terminal for raw input
 *
 * Raw mode disables canonical mode and echo, allowing us to process each
 * keypress as it happens. This function:
 * 1. Saves original terminal attributes
 * 2. Registers cleanup function via atexit()
 * 3. Configures terminal flags for raw mode
 * 4. Applies the new settings
 *
 * Terminal Attribute Modification Process:
 * - Read current attributes with tcgetattr() into a struct
 * - Modify the struct by hand
 * - Write new attributes with tcsetattr()
 */
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("tcgetattr");

  // atexit() comes from <stdlib.h>.
  // we use it to register our disableRawMode() function to be called
  // automatically when the program exits, whether it exits by return from
  // main(), or by calling the exit() function. this ensures, we'll leave the
  // terminal attributes the way we found them when our program exits.
  atexit(disableRawMode);

  // We can set a terminal's attributes by
  // 1) using tcgetattr() to read the current attributes into a struct
  // 2) modifying the struct by hand and
  // 3) passing the modified struct to tcsetattr() to write the new terminal
  // attributes back out.

  // struct termios, tcgetattr(), tcsetattr(), ECHO, and TCSAFLUSH all come from
  // <termios.h>.

  struct termios raw = E.orig_termios;

  // Terminal attributes can be read into a termios struct by tcgetattr().
  // the c_lflag field is for "local flags". the other flags fields are
  // c_iflag (input flags), c_oflag (output flags), and c_cflag (control flags),
  // all of which we will have to modify to enable raw mode
  // turning off canonical mode

  // ICRNL comes from <termios.h>. The I stands for “input flag”, CR stands for
  // "carriage return", and NL stands for "new line". (basically we are fixing
  // Ctrl-M here)

  /*
   * Input Flags (c_iflag) - Control input processing
   *
   * Disabling:
   * - BRKINT : Break condition sends SIGINT (like Ctrl-C)
   * - ICRNL  : Translate carriage return to newline (fixes Ctrl-M)
   * - INPCK  : Enable parity checking
   * - ISTRIP : Strip 8th bit of input bytes
   * - IXON   : Enable software flow control (Ctrl-S/Ctrl-Q)
   *
   * Historical note: Ctrl-S/Ctrl-Q (XOFF/XON) were used to pause/resume
   * transmission to slow devices like printers.
   */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  // by default, Ctrl-S and Ctrl-Q are used for software flow control. Ctrl-S
  // stops data from being transmitted to the terminal until you press Ctrl-Q.
  // fun fact: this originates in the days when you might want to pause the
  // transmission of data to let a device like printer catch up. IXON comes from
  // <termios.h>. The I stands for “input flag” and XON comes from teh n and
  // ames of the two control characters that Ctrl-S Ctrl-Q produce: XOFF to
  // pause transmission and XON to resume transmission.

  // turn off all output processing features by turning off the OPOST flag.

  /*
   * Output Flags (c_oflag) - Control output processing
   *
   * Disabling:
   * - OPOST : Turn off all output processing (e.g., \n to \r\n translation)
   */
  raw.c_oflag &= ~(OPOST);

  // BRKINT, INPCK, ISTRIP, and CS8 all come from <termios.h>.
  // When BRKINT is turned on, a break condition will cause a SIGINT signal to
  // be sent to the program, like pressing Ctrl-C. INPCK enables parity
  // checking, ISTRIP causes the 8th bit of each input byte to be stripped,
  // meaning it will set it to 0. CS8 is not a flag, it is a bit mask with
  // multiple bits, which we set using the bitwise-OR (|) operator unlike all
  // the flags we are turning off. It sets the character size (CS) to 8 bits per
  // byte.

  /*
   * Control Flags (c_cflag) - Control hardware settings
   *
   * Setting:
   * - CS8 : Set character size to 8 bits per byte
   *
   * Note: CS8 is a bit mask (not a single flag), so we use OR (|) to set it
   * rather than AND-NOT (~) like the flags we're disabling.
   */
  raw.c_cflag |= (CS8);

  // added IEXTEN flag to disable Ctrl-V

  /*
   * Local Flags (c_lflag) - Control local terminal behavior
   *
   * Disabling:
   * - ECHO   : Don't echo typed characters to the terminal
   * - ICANON : Disable canonical mode (line-by-line input)
   * - IEXTEN : Disable extended input processing (Ctrl-V)
   * - ISIG   : Disable signal generation (Ctrl-C = SIGINT, Ctrl-Z = SIGTSTP)
   *
   * Bitwise operation explained:
   * Each flag is a bit in the field. We use bitwise-NOT (~) to flip all bits
   * of the flag, then AND (&) with the field to clear just that bit while
   * preserving all others.
   *
   * Note: Despite starting with 'I', ICANON and ISIG are local flags, not
   * input flags (those are in c_iflag).
   */

  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  // ECHO is a bitflag, defined as 00000000000000000000000000001000 in binary.
  // we use the bitwise-NOT operator (~) on this value to get
  // 11111111111111111111111111110111. we then bitwise-AND this value with the
  // flags field, which forces the fourth bit in the flags to become 0, and
  // causes every other bit to retain its current value.

  // ICANON comes from <termios.h>. Input flags (the ones in the c_iflag field)
  // generally start with I like ICANON does. However, ICANON is not an input
  // flag, it’s a “local” flag in the c_lflag field.

  // By default, Ctrl-C sends a SIGINT signal to the current process which
  // causes it to terminate, and Ctrl-Z sends a SIGTSTP signal to the current
  // process which causes it to suspend. ISIG comes from <termios.h>. Like
  // ICANON, it starts with I but isn’t an input flag.

  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  // VMIN and VTIME come from <termios.h>. They are indexes into the c_cc field,
  // which stands for “control characters”, an array of bytes that control
  // various terminal settings. VMIN value sets the minimum number of bytes of
  // input needed before read() can return(). we set it to 0 so that read()
  // returns as soon as there is any input to be read. VTIME value sets the
  // maximum amount of time to wait before read() returns. it is in tenths of a
  // second, so we set it to 1/10 of second, or 100ms. if read() times out, it
  // will return 0, which makes sense because its usual return value is the
  // number of bytes read.

  // after the attributes have been modified, you can then apply them to the
  // terminal using tcsetattr(). the TCSAFLUSH argument specifies when to apply
  // the change: in this case, it waits for all pending output to be written to
  // the terminal, and also discards any input that hasn’t been read.
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

/*
 * editorReadKey() - Wait for and return a single keypress
 *
 * Reads from stdin in a loop until a character is received.
 * Handles EAGAIN errors (expected from non-blocking reads with timeout).
 * Dies on any other read error.
 */
int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if(c == '\x1b') {
    char seq[3];

    if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if(seq[0] == '[') {
      switch (seq[1]) {
        case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
      }
    }

   return '\x1b';
  } else {
    return c;
  }
}

/*
 * getCursorPosition() - Query terminal for cursor position
 *
 * Uses the Device Status Report escape sequence (<esc>[6n) to query the
 * terminal for the current cursor position. The terminal responds with
 * <esc>[rows;colsR which we parse using sscanf().
 *
 * This is used as a fallback method for getting window size when ioctl fails.
 *
 * Returns: 0 on success, -1 on failure
 */
int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1)
      break;
    if (buf[i] == 'R')
      break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[')
    return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
    return -1;

  return 0;
}

/*
 * getWindowSize() - Get terminal window dimensions
 *
 * Primary method: Uses ioctl() with TIOCGWINSZ to query terminal size.
 * Fallback method: Positions cursor at bottom-right, then queries cursor
 * position.
 *
 * Fallback strategy:
 * 1. Send <esc>[999C (Cursor Forward) and <esc>[999B (Cursor Down)
 * 2. Large value (999) ensures cursor reaches edge of screen
 * 3. C and B commands stop at screen edge (won't go past)
 * 4. Query cursor position to determine actual screen dimensions
 *
 * We use this approach instead of <esc>[999;999H because the documentation
 * doesn't specify behavior for off-screen positioning.
 *
 * Note: ioctl() isn't guaranteed to work on all systems, hence the fallback.
 *
 * Returns: 0 on success, -1 on failure
 */
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

// ioctl() isn’t guaranteed to be able to request the window size on all
// systems, so we are going to provide a fallback method of getting the window
// size.

// the strategy is to position the cursor at the bottom-right of the screen,
// then use escape sequences that let us query the position of the cursor. That
// tells us how many rows and columns there must be on the screen.

// We are sending two escape sequences one after the other. The C command
// (Cursor Forward) moves the cursor to the right, and the B command (Cursor
// Down) moves the cursor down. The argument says how much to move it right or
// down by. We use a very large value, 999, which should ensure that the cursor
// reaches the right and bottom edges of the screen.

// The C and B commands are specifically documented to stop the cursor from
// going past the edge of the screen. The reason we don’t use the <esc>[999;999H
// command is that the documentation doesn’t specify what happens when you try
// to move the cursor off-screen.

/*** append buffer ***/
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

// an append buffer consists of a pointer to our buffer in memory, and a length.
// we define ABUF_INIT constant which represents an empty buffer. This acts as a
// constructor for our abuf type.

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) { free(ab->b); }

// realloc() and free() come from <stdlib.h>. memcpy() comes from <string.h>.
// To append a string s to an abuf, the first thing we do is make sure we
// allocate enough memory to hold the new string. We ask realloc() to give us a
// block of memory that is the size of the current string plus the size of the
// string we are appending. realloc() will either extend the size of the block
// of memory we already have allocated, or it will take care of free()ing the
// current block of memory and allocating a new block of memory somewhere else
// that is big enough for our new string.
//
// then we use memcpy() to copy the string s after the end of the current data
// in the buffer, and we update pointer and length of the abuf to the new
// values.
//
// abFree() is a destructor that deallocates the dynamic memory user by an abuf.

/*** output ***/

/*
 * Display and Rendering Functions
 *
 * This section handles all screen output including:
 * - Drawing rows of tildes (like vim)
 * - Screen refresh and clearing
 */

/*
 * editorDrawRows() - Draw tilde rows on screen
 *
 * Draws a column of tildes (~) down the left side of the screen, similar to
 * vim's display for lines past the end of the buffer.
 */
void editorDrawRows(struct abuf *ab) {
  int y;

  for (y = 0; y < E.screenrows; y++) {
    if (y == E.screenrows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
                                "Kilo editor -- version %s", KILO_VERSION);
      if (welcomelen > E.screencols)
        welcomelen = E.screencols;
      int padding = (E.screencols - welcomelen) / 2;
      if (padding) {
        abAppend(ab, "~", 1);
        padding--;
      }
      while (padding--)
        abAppend(ab, " ", 1);
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "~", 1);
    }

    abAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

/*
 * editorRefreshScreen() - Clear screen and redraw
 *
 * Escape sequences used:
 * - <esc>[2J : Clear entire screen (J command with arg 2)
 * - <esc>[H  : Position cursor at top-left (H command, default 1;1)
 *
 * VT100 Escape Sequence Primer:
 * - Format: <esc>[<args><command>
 * - <esc> is byte 27 (0x1b in hex)
 * - Followed by '[' character
 * - Optional numeric arguments
 * - Command letter
 *
 * J command variants:
 * - <esc>[0J or <esc>[J : Clear from cursor to end of screen
 * - <esc>[1J            : Clear from cursor to beginning of screen
 * - <esc>[2J            : Clear entire screen
 *
 * H command:
 * - Positions cursor at specified row;column
 * - No arguments defaults to 1;1 (top-left)
 *
 * We use VT100 sequences for broad terminal emulator compatibility.
 */
void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}
// write() and STDOUT_FILENO come from <unistd.h>.
// the 4 in our write() call means we are writing 4 bytes out to the terminal.
// the first byte it \x1b, which is the escape character, or 27 in decimal.
// the other 3 bytes are [2J

// second escape sequence is only 3 bytes long, and uses the H command (cursor
// position) to position the cursor. the H command actually takes two arguments:
// the row number and the column number at which to position the cursor.

// extra info:
// we are writing an escape sequence to the terminal. escape sequences always
// starts with an escape character (27) followed by a [ character. escape
// sequences instruct the terminal to do various text formatting tasks, such as
// coloring text, moving the cursor around, and clearing parts of th screen.
//
// we are using the J command (erase in display) to clear the screen. escape
// sequence commands take args which come before the command. in this case the
// arg is 2, which says to clear the entire screen. <esc>[1J would clear the
// screen from the cursor up to the end of the screen. also 0 is the default arg
// for J, so just <esc>[J by itself would also clear the screen from the cursor
// to the end.
//
// for this text editor we will mostly be using VT100 escape sequences, which
// are supported very widely by modern terminal emulators.
//
// we use escape sequences to tell the terminal to hide and show the cursor.
// the h and l commands (Set Mode, Reset Mode) are used to turn on and turn
// off various terminal features or “modes”.

/*** input ***/

void editorMoveCursor(int key) {
  switch (key) {
  case ARROW_LEFT:
    if (E.cx != 0) {
      E.cx--;
    }
    break;
  case ARROW_RIGHT:
    if (E.cx != E.screencols - 1) {
      E.cx++;
    }
    break;
  case ARROW_UP:
    if (E.cy != 0) {
      E.cy--;
    }
    break;
  case ARROW_DOWN:
    if (E.cy != E.screenrows - 1) {
      E.cy++;
    }
    break;
  }
}

/*
 * Input Processing Functions
 *
 * This section handles user input and keypress processing.
 */

/*
 * editorProcessKeypress() - Handle user input
 *
 * Waits for a keypress and processes it.
 * Currently handles:
 * - Ctrl-Q: Clear screen and exit
 */
void editorProcessKeypress() {
  int c = editorReadKey();

  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;

  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;
  }
}

/*
 * initEditor() - Initialize editor state
 *
 * Sets up the global editor configuration, including detecting the current
 * terminal window dimensions.
 */
void initEditor() {
  E.cx = 0;
  E.cy = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
}

/*** init ***/
int main() {
  enableRawMode();
  initEditor();
  // initEditor()’s job will be to initialize all the fields in the E struct.

  // read() and STDIN_FILENO come from <unistd.h>.
  // we are asking read() to read 1 byte from standard input into the variable c
  // and to keep it doing until there are no more bytes to read.
  // read() returns the numbers of bytes that it read, and will return 0 when it
  // reaches the end of a file

  // iscntrl() comes from <ctype.h>, and printf() comes from <stdio.h>.
  // iscntrl() tests whether a character is a control character. Control
  // characters are nonprintable characters that we don't want to print to the
  // screen. ASCII codes 0-31 are all control characters, and 127 is also a
  // control character. ASCII codes 32-216 are all printable.
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  // added a timeout for read(), so that read() returns if it doesn’t get any
  // input for a certain amount of time.

  // errno and EAGAIN come from <errno.h>.
  // tcsetattr(), tcgetattr(), and read() all return -1 on failure, and set the
  // errno value to indicate the error.

  // printf() can print multiple representations of a byte. %d tells it to
  // format the byte as a decimal number (it's ASCII code), and %c tells it to
  // write out the byte directly, as a character.
  return 0;
}
