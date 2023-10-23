#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <errno.h>

#define CTRL_KEY(k) ((k) & 0x1f)
//keeping the original termios struct for reseting the changes of terminal when program exits.
struct termios original_termios;


void die(const char *s){
    perror(s);
    exit(1);
}

/*
    disbales the changes of enableRawMode function(sets the attributes to the original_termios).
*/
void disableRawMode(){
    if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&original_termios) == -1)
    die("tcsetattr");
}

/*
this function turns off the echoing mode and cannonical mode and some keys functionality (CTRL + ...) and enabling the raw mode
*/
void enableRawMode(){

    //getting the terminal attributes and keeping it in raw struct
    if(tcgetattr(STDIN_FILENO,&original_termios) == -1) die("tcgetattr");

    //runnig the disableRawMode function in exit
    atexit(disableRawMode);

    struct termios raw = original_termios;

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
/*
this function processes that if the entered key is a CTRL key or is a regular one (idk what to write)
and executes the process of it . 
*/
void editorProcessKeypress(){
    char c = editorReadKey();

    switch(c){
        case CTRL_KEY('q'):
            exit(0);
            break;

    }
}
int main(){
    enableRawMode();

    while (1)
    {
        editorProcessKeypress();
    }
    return 0;
}