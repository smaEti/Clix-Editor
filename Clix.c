#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define CLIX_VERSION "0.0.1"
#define CLIX_TAB_STOP 8
// returns the ACII code for the combination of the given charachter and the
// CTRL
#define CTRL_KEY(k) ((k)&0x1f)

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN

};
// stands for Editor's row
typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
} erow;

// keeping the original termios struct for reseting the changes of terminal when
// program exits.
struct editorConfig {
  // cx ,cy is the cursor's position
  int cx, cy;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int numrows;
  erow *row;
  struct termios original_termios;
};

struct editorConfig E;

void die(const char *s) {
  // the 4 means we want to write 4 bytes.
  //\x1b is an escape character and command J is for Erase In display
  write(STDOUT_FILENO, "\x1b[2J", 4);
  // H is for cursor position
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}

/*
    disbales the changes of enableRawMode function(sets the attributes to the
   original_termios).
*/
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_termios) == -1)
    die("tcsetattr");
}

/*
this function turns off the echoing mode and cannonical mode and some keys
functionality (CTRL + ...) and enabling the raw mode
*/
void enableRawMode() {

  // getting the terminal attributes and keeping it in raw struct
  if (tcgetattr(STDIN_FILENO, &E.original_termios) == -1)
    die("tcgetattr");

  // runnig the disableRawMode function in exit
  atexit(disableRawMode);

  struct termios raw = E.original_termios;

  // turning off the ECHO mode (basicly after this line of code your terminal
  // does not show you what you write) turning off the canonical mode (now
  // terminal will be reading input byte-by-byte,instead of line-by-line) SIGINT
  // and SIGTSTP (CTRL + (C + Z)) are two signals to terminate and suspend the
  // proccess in the termianl and it'll be disabeld after this line of code
  // IEXTEN is for (CTRL + V)
  // c_lflag is for local flags
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

  // c_iflag is for input flags
  // IXON : disabling (CTRL + S) and (CTRL + Q)
  // ICTRNL : CTRL + M
  raw.c_iflag &= ~(IXON | ICRNL | ISTRIP | BRKINT | INPCK);

  // Turn off all output processing
  raw.c_oflag &= ~(OPOST);

  raw.c_cflag |= (CS8);

  // timeout for read()
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  // set termial attributes that is changed in raw struct back
  // TCSAFLUSH : specifies when to apply the change,it waits for all pending
  // output to-
  //  -be written to the terminal,and also discards any input that hasn't been
  //  read.
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}
/*
its a function on the high level of the program for reading the keyPasses*/
int editorReadKey() {
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
}

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

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  // TIOCGWINSZ = Terminal IO get Win Size (maybe)
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    // goes to 999x999 block of the terminal (it goes to the right-bottom of
    // terminal)
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;

    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}
/*
uses the chars string of an erow to fill in the contents of the render string.
it copies each character from chars to render.
*/
void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  // loops through the chars of the row and count the tabs in order to know how
  // much memory to allocate for render
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t')
      tabs++;

  free(row->render);
  row->render = malloc(row->size + tabs * (CLIX_TAB_STOP - 1) + 1);

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % CLIX_TAB_STOP != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }

  row->render[idx] = '\0';
  // idx contains the number of characters we copied into row->render
  row->rsize = idx;
}

/*
allocates string for each row and writes the each row in it;
*/
void editorAppendRow(char *s, size_t len) {
  // allocating the rows
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);

  E.row[at].chars[len] = '\0';
  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
}

/* this function is for opening and reading a file from disk*/
void editorOpen(char *filename) {
  // open file
  FILE *fp = fopen(filename, "r");
  if (!fp)
    die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  // reads every row of file and calls editorAppendRow for everyline to append
  // the row in buffer
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen);
  }

  free(line);
  fclose(fp);
}

/*** append buffer ***/
struct abuf {
  char *b;
  int len;
};
#define ABUF_INIT                                                              \
  { NULL, 0 }

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}
void abFree(struct abuf *ab) { free(ab->b); }

/*checks the editor (to be initalized and gets the cols and rows number) and
 * lets the app to initialize*/
void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
}

/* checks if the cursor is going out of the window and adds the value to rowoff
 * for scrolling */
void editorScroll() {
  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.cx < E.coloff) {
    E.coloff = E.cx;
  }
  if (E.cx >= E.coloff + E.screencols) {
    E.coloff = E.cx - E.screencols + 1;
  }
}

/*prints ~ in the start of every line (the commented section is for numbers)*/
void editorDrawRows(struct abuf *ab) {
  int y;
  // looping on each row
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      // if there is not any numrows (files) shows welcome page otherwise it
      // prints ~ to console
      if (E.numrows == 0 && y == E.screenrows / 3) {
        // Wellcome page :
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "Clix editor -- version %s", CLIX_VERSION);
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

      } else { // OTHERWISE
        // appends ~ to the buffer we made
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      /*when subtracting E.coloff from the length, len can now be a negative
       number, meaning the user scrolled horizontally past the end of the line.
       In that case, we set len to 0 so that nothing is displayed on that
       line.*/
      if (len < 0)
        len = 0;
      if (len > E.screencols)
        len = E.screencols;
      abAppend(ab, &E.row[filerow].render[E.coloff], len);
    }

    abAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

// clears the whole screen (terminal)
void editorRefreshScreen() {
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  // the 6 means we want to write 6 bytes.
  //\x1b is an escape character and command J is for Erase In display
  //  abAppend(&ab, "\x1b[2J", 4);
  //  H is for cursor position
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  // setting the first position of cursor in screen
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
           (E.cx + E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}
void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key) {
  case ARROW_LEFT:
    if (E.cx != 0) {
      E.cx--;
    } else if (E.cy > 0) { // Allow user to use <- at beginning of the line to
                           // move to end of previous line
      E.cy--;
      E.cx = E.row[E.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && E.cx < row->size) {
      E.cx++;
    } else if (row &&
               E.cx ==
                   row->size) { // allow the user to press → at the end of a
                                // line to go to the beginning of the next line.
      E.cy++;
      E.cx = 0;
    }
    break;
  case ARROW_UP:
    if (E.cy != 0) {
      E.cy--;
    }
    break;
  case ARROW_DOWN:
    if (E.cy < E.numrows) {
      E.cy++;
    }
    break;
  }
  /*set E.cx to the end of that line if E.cx is to the right of the end of that
  line. we consider a NULL line to be of length 0.*/
  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}
/*
this function processes that if the entered key is a CTRL key or is a regular
one (idk what to write) and executes the process of it .
*/
void editorProcessKeypress() {
  int c = editorReadKey();

  switch (c) {
  case CTRL_KEY('q'):
    // the 4 means we want to write 4 bytes.
    //\x1b is an escape character and command J is for Erase In display
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // H is for cursor position
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;

  case HOME_KEY:
    E.cx = 0;
    break;

  case END_KEY:
    E.cx = E.screenrows - 1;
    break;
  case ARROW_UP:
  case ARROW_DOWN:
  case ARROW_LEFT:
  case ARROW_RIGHT:
    editorMoveCursor(c);
    break;
  case PAGE_UP:
  case PAGE_DOWN: {
    int times = E.screenrows;
    while (times--) {
      editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
    }
    break;
  }
  }
}
int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
// step 85