#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>

//returns the ACII code for the combination of the given charachter and the CTRL
#define CTRL_KEY(k) ((k) & 0x1f)

//keeping the original termios struct for reseting the changes of terminal when program exits.
struct editorConfig
{
    int screenrows;
    int screencols;
    struct termios original_termios;
};

struct editorConfig E;

void die(const char *s){
    //the 4 means we want to write 4 bytes.
    //\x1b is an escape character and command J is for Erase In display
    write(STDOUT_FILENO,"\x1b[2J",4);
    // H is for cursor position
    write(STDOUT_FILENO,"\x1b[H",3);
    perror(s);
    exit(1);
}

/*
    disbales the changes of enableRawMode function(sets the attributes to the original_termios).
*/
void disableRawMode(){
    if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&E.original_termios) == -1)
    die("tcsetattr");
}

/*
this function turns off the echoing mode and cannonical mode and some keys functionality (CTRL + ...) and enabling the raw mode
*/
void enableRawMode(){

    //getting the terminal attributes and keeping it in raw struct
    if(tcgetattr(STDIN_FILENO,&E.original_termios) == -1) die("tcgetattr");

    //runnig the disableRawMode function in exit
    atexit(disableRawMode);

    struct termios raw = E.original_termios;

    //turning off the ECHO mode (basicly after this line of code your terminal does not show you what you write)
    //turning off the canonical mode (now terminal will be reading input byte-by-byte,instead of line-by-line)
    //SIGINT and SIGTSTP (CTRL + (C + Z)) are two signals to terminate and suspend the proccess in the termianl and it'll be disabeld after this line of code
    //IEXTEN is for (CTRL + V)
    //c_lflag is for local flags
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    //c_iflag is for input flags
    //IXON : disabling (CTRL + S) and (CTRL + Q)
    //ICTRNL : CTRL + M
    raw.c_iflag &= ~(IXON | ICRNL | ISTRIP | BRKINT | INPCK);

    //Turn off all output processing
    raw.c_oflag &= ~(OPOST);

    raw.c_cflag |= (CS8);

    //timeout for read()
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    
    //set termial attributes that is changed in raw struct back
    //TCSAFLUSH : specifies when to apply the change,it waits for all pending output to-
    // -be written to the terminal,and also discards any input that hasn't been read. 
    if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw) == -1) die("tcsetattr");
}
/*
its a function on the high level of the program for reading the keyPasses*/
char editorReadKey(){
    int nread;
    char c;

    while(( nread = read(STDIN_FILENO , &c, 1)) != 1){ 
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    return c;
}

int getCursorPosition(int *rows, int *cols) {

  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  while (i < sizeof(buf) - 1) 
  {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  
  return 0;
}

int getWindowSize(int *rows,int *cols){
    struct winsize ws;

    //TIOCGWINSZ = Terminal IO get Win Size (maybe)
    if (ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws) == -1 || ws.ws_col == 0)
    {
        //goes to 999x999 block of the terminal (it goes to the right-bottom of terminal)
        if(write(STDOUT_FILENO,"\x1b[999C\x1b[999B",12) != 12) return -1;

        return getCursorPosition(rows, cols);
    }else{
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
    
}

/*checks the editor (to be initalized and gets the cols and rows number) and lets the app to initialize*/
void initEditor(){
    if(getWindowSize(&E.screenrows,&E.screencols) == -1) die("getWindowSize");
}

/*prints ~ in the start of every line (the commented section is for numbers)*/
void editorDrawRows() {
  int y;
//   int x = 1;
  for (y = 0; y < E.screenrows; y++) {
    write(STDOUT_FILENO,"~",1);
    // char buffer[80];
    // int lengthUsed = sprintf(buffer,"%d",x);
    // write(STDOUT_FILENO, buffer , lengthUsed);
    // write(STDOUT_FILENO, "~", 1);
    // write(STDOUT_FILENO,"\r\n",2);
    if (y < E.screenrows - 1)
    {
        write(STDOUT_FILENO,"\r\n",2);
    }
    // x++;
  }
}


//clears the whole screen (terminal)
void editorRefreshScreen(){
    //the 4 means we want to write 4 bytes.
    //\x1b is an escape character and command J is for Erase In display
    write(STDOUT_FILENO,"\x1b[2J",4);
    // H is for cursor position
    write(STDOUT_FILENO,"\x1b[H",3);

    editorDrawRows();
    
    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*
this function processes that if the entered key is a CTRL key or is a regular one (idk what to write)
and executes the process of it . 
*/
void editorProcessKeypress(){
    char c = editorReadKey();

    switch(c){
        case CTRL_KEY('q'):
            //the 4 means we want to write 4 bytes.
            //\x1b is an escape character and command J is for Erase In display
            write(STDOUT_FILENO,"\x1b[2J",4);
            // H is for cursor position
            write(STDOUT_FILENO,"\x1b[H",3);
            exit(0);
            break;

    }
}
int main(){
    enableRawMode();
    initEditor();

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}