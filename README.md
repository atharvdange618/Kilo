# Kilo Text Editor 📝

> Building a text editor from scratch in C - A learning journey

This is my implementation of **kilo**, a minimal text editor written in C, following the excellent [Build Your Own Text Editor](https://viewsourcecode.org/snaptoken/kilo/index.html) tutorial by snaptoken.

## 🎯 Project Goals

- **Fun Revisit to C programming** through hands-on systems programming
- **Understand terminal control** and raw mode input handling
- **Build something real** - a fully functional text editor (~1000 lines of C)
- **No dependencies** - just pure C and the standard library

## ✨ Features

### Currently Implemented

- ✅ **Raw Mode Terminal Handling**
  - Custom terminal flag configuration (termios)
  - Non-blocking input with timeout (VMIN/VTIME)
  - Disabled flow control (Ctrl-S/Ctrl-Q)
  - Signal handling override (Ctrl-C/Ctrl-Z)
  - Proper carriage return handling

## 🚀 Getting Started

### Prerequisites

- GCC compiler or any C compiler
- Linux/Unix terminal (uses termios.h)
- Make

### Building

```bash
# Using make
make

# Or compile directly
cc kilo.c -o kilo -Wall -Wextra -pedantic -std=c99
```

### Running

```bash
./kilo
```

Press `q` to quit. The program currently demonstrates raw mode input by displaying ASCII codes of pressed keys.

## 📚 What I'm Learning

This project is teaching me:

- **Low-level terminal control** - How terminals actually work under the hood
- **C programming patterns** - System calls, bitwise operations
- **POSIX APIs** - termios, file descriptors, signal handling
- **Build systems** - Makefiles and compilation flags
- **Systems programming** - Direct interaction with OS APIs

### Interesting Discoveries

- Ctrl-S and Ctrl-Q historically controlled data flow to slow devices like printers!
- Terminal "cooked mode" vs "raw mode" - most terminal apps use raw mode
- The complexity behind simple key presses - flags, timeouts, special character handling
- How `\r\n` (carriage return + newline) is needed in raw mode vs just `\n` in cooked mode

## 🛠️ Technical Details

**Language:** C (C99 standard)
**Lines of Code:** Growing towards ~1000 LOC
**Architecture:** Single-file implementation
**Dependencies:** None (standard library only)

### Key Components

- `enableRawMode()` - Configures terminal for raw input
- Terminal flag configuration (c_iflag, c_oflag, c_cflag, c_lflag)
- Input loop with timeout handling

## 📖 Tutorial Progress

Following the 184-step tutorial from [viewsourcecode.org/snaptoken/kilo](https://viewsourcecode.org/snaptoken/kilo/index.html)

**Current Step:** Early stages - Raw mode input complete

## 🙏 Credits

- **Original kilo editor** by [antirez](https://github.com/antirez/kilo)
- **Tutorial** by [snaptoken](https://viewsourcecode.org/snaptoken/kilo/)
- Inspired by the philosophy of learning by building

## 📝 Development Log

**2026-01-14:**

- Enhanced raw mode with comprehensive terminal flag configuration
- Added VMIN/VTIME for non-blocking reads
- Implemented detailed documentation of termios flags

**2026-01-13:**

- Implemented basic raw mode terminal input handling
- Set up Makefile for easy compilation
- Initial project setup

## 🔗 Resources

- [Build Your Own Text Editor Tutorial](https://viewsourcecode.org/snaptoken/kilo/index.html)
- [Original kilo repository](https://github.com/antirez/kilo)
- [termios man page](https://man7.org/linux/man-pages/man3/termios.3.html)

## 📄 License

This is a learning project following a public tutorial. The original kilo editor is released under the BSD 2-Clause License.

---

**Status:** 🚧 Work in Progress - Building something amazing, one step at a time!
