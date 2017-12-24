// test_terminal.cpp : Defines the entry point for the console application.
//

// vc++ testing headers

#define SAVE_SCREEN
#define USE_TI92PLUS
#define USE_V200
#define MIN_AMS 200
#define HORIZ_SPACING 6
#define VERT_SPACING 8
#include <tigcclib.h>

//#include "vt52e.h"
// TIGCC isn't finding my header file. it's in the folder and project. Bodge: pasted contents here

typedef enum { false, true } bool;

void reset_buffer(void);
void reset_escape_buffer(void);
void init_buffer(void);
void free_buffer(void);
void shift_up(void);
void shift_down(void);
void next_cursor(bool do_shift);
void prev_cursor(bool do_shift);
void up_cursor(void);
void down_cursor(void);
void newline(void);
unsigned short int cursor_pos(void);
void newchar(char c);
void print_char(char c);
void backspace();
void escape_buffer_handler(char c);
void _draw_display(void);
void handle_dirty(void);
int senddata(char c);
int sendbuffer(char* c);
int recvdata();
int main(void);

// /header

// define buffer dimensions -- these should fit a 92+ nicely
#define MAX_ROW 16
#define MAX_COL 40
#define MAX_CHR MAX_ROW*MAX_COL
// what's the most data we want to hold in the escape buffer
#define MAX_EBS 4

// buffer access vars
unsigned short int xpos;
unsigned short int ypos;
char* buffer;
char* escape_buffer;
unsigned char* recv_buffer;
bool escape_buffer_active;
unsigned short int eb_i;
bool dirty; // redraw display if this is true

void reset_buffer(void) {
  //short int i;
  //for(i = 0; i < MAX_CHR; i++) buffer[i] = 0x20;
  memset(buffer, (char)0x20, sizeof(buffer));
  memset(buffer+sizeof(buffer)-1, 0, sizeof(char));
  memset(escape_buffer, (char)0, sizeof(escape_buffer));
  memset(recv_buffer, (char)0, sizeof(recv_buffer));
  xpos = 0;
  ypos = 0;
}

void reset_escape_buffer(void) {
  escape_buffer_active = false;
  memset(escape_buffer, 0, sizeof(escape_buffer));
  eb_i = 0;
}

void init_buffer(void) {
  buffer = (char*)calloc(MAX_ROW*MAX_COL+1, sizeof(char));
  escape_buffer = (char*)calloc(MAX_EBS, sizeof(char));
  recv_buffer = (unsigned char*)calloc(255, sizeof(unsigned char));
  reset_buffer();
}

void free_buffer(void) {
  free(buffer);
  free(escape_buffer);
  free(recv_buffer);
}

// buffer cursor routines

void shift_up(void) {
//  _draw_display();
  memmove(buffer, buffer+sizeof(char)*MAX_COL, sizeof(char)*(MAX_CHR-MAX_COL));
  memset(buffer+sizeof(char)*(MAX_CHR-MAX_COL), 0x20, sizeof(char)*MAX_COL);
  dirty = true;
//  _draw_display();
}

void shift_down(void) {
  memmove(buffer+sizeof(char)*MAX_COL, buffer, sizeof(char)*(MAX_CHR-MAX_COL));
  memset(buffer, 0x20, sizeof(char)*MAX_COL);
  dirty = true;
}

void next_cursor(bool do_shift) {
  if(ypos == MAX_ROW && do_shift) {
    shift_up();
    ypos--;
  }
  if(xpos == MAX_COL-1) {
    ypos++;
    xpos = 0;
    return;
  }
  else xpos += 1;
}

void prev_cursor(bool do_shift) {
  if(ypos == 0 && do_shift) {
    shift_up();
    ypos++;
  }
  if(xpos == 0) {
    ypos--;
    xpos = MAX_COL-1;
    return;
  }
  else xpos += 1;
}

void up_cursor(void) {
  if(ypos > 0) ypos--;
}
  
void down_cursor(void) {
  if(ypos < MAX_ROW-1) ypos++;
}

void newline(void) {
  if(ypos < MAX_ROW-1) ypos++;
  else shift_up();
}

unsigned short int cursor_pos(void) {
  return ypos*MAX_COL+xpos;
}

// buffer i/o routines

void newchar(char c) {
  if(c >= (char)0x20 && !escape_buffer_active) { //printable
    print_char(c);
  }
  else if(c == 0x1b || escape_buffer_active) { //escape codes
    escape_buffer_active = true;
    escape_buffer_handler(c);
  }
  else if(c < 0x20) { // parse control characters
    switch(c) {
    case 0x0a: //\n
      newline();
      break;
    case 0x13: //\r
      xpos = 0;
      break;
    default:
      break;
    }
  }
}

void print_char(char c) {
  if(ypos == MAX_ROW) {
    shift_up();
    ypos--;
  }
  buffer[ypos*MAX_COL+xpos] = c;
  DrawChar(xpos*HORIZ_SPACING, ypos*VERT_SPACING, buffer[ypos*MAX_COL+xpos], A_REPLACE);
  if(xpos == MAX_COL-1) {
    ypos++;
    xpos = 0;
    return;
  }
  else xpos += 1;
}

void backspace() {
  prev_cursor(false);
  newchar(' ');
}

void escape_buffer_handler(char c) {
  escape_buffer[eb_i] = c;
  eb_i += 1;
  if(eb_i == 2 && escape_buffer[1] != 'Y') {
    switch(escape_buffer[1]) {
    case 0x41:
      // ESC A: move cursor up, do not scroll
      if(ypos > 0) ypos--;
      break;
    case 0x42:
      // ESC B: move cursor down, do scroll
      newline();
      break;
    case 0x43:
      // ESC C: move cursor right
      next_cursor(false);
      break;
    case 0x44:
      // ESC D: move cursor left
      prev_cursor(false);
      break;
    case 0x48:
      // ESC H: set cursor to (0, 0)
      xpos = 0;
      ypos = 0;
      break;
    case 0x49:
      // ESC I: insert blank line above cursor, move cursor up
      break;
    case 0x4a:
      // ESC J: erase from cursor to end of screen
      memset(buffer+cursor_pos(), 0x20, sizeof(buffer)-cursor_pos());
      break;
    case 0x4b:
      // ESC K: erase from cursor to end of line
      break;
    case 0x4c:
      // ESC L: insert a line (at cursor? shift?)
      break;
    case 0x4d:
      // ESC M: delete a line (at cursor? shift?)
      break;
    case 0x5a:
      // ESC Z: identify (we're supposedly a VT52):
      //send(0x1b, 0x2f, 0x4b);
      break;
    }
    reset_escape_buffer();
    return;
  }
  else if(escape_buffer[1] == 'Y' && eb_i == 4) {
    xpos = (unsigned short int)escape_buffer[2]-31;
    ypos = (unsigned short int)escape_buffer[3]-31;
    if(xpos>MAX_COL-1) xpos = 0;
    if(ypos>MAX_ROW-1) ypos = 0;
    reset_escape_buffer();
    return;
  }
  else if(escape_buffer[1] != 'Y' && eb_i >= 2) {
    reset_escape_buffer();
    return;
  }
}


// debugging routines

void _draw_display(void) {
  short int j;
  char* m;
  m = (char*)calloc(MAX_COL+1, sizeof(char));
  for(j=0; j < MAX_ROW; j++) {
    //for(i=0; i < MAX_COL; i++) {
    //  DrawChar(j*HORIZ_SPACING, i*VERTICAL_SPACING, buffer[j*MAX_COL+i], A_REPLACE); // use DrawStr?
    //}
    memcpy(m, buffer+j*MAX_COL, sizeof(char)*MAX_COL);
    DrawStr(j*HORIZ_SPACING, 0, m, A_REPLACE);
  }
  free(m);
}

void handle_dirty(void) {
  _draw_display();
  dirty = false;
}

// i/o

int senddata(char c) {
  return LIO_SendData(&c, sizeof(char));
}

int sendbuffer(char* c) {
  return LIO_SendData(c, sizeof(char));
}

int recvdata() {
  memset(recv_buffer, 0, sizeof(recv_buffer));
  return LIO_RecvData(recv_buffer, sizeof(recv_buffer), 2);
}


int __main(void) {
  char c;
	unsigned short int k;
	void* q_kb = kbd_queue();
	ClrScr();
  FontSetSys(F_6x8);
  init_buffer();
  reset_buffer();
  while(true) {
		if(!OSdequeue(&k, q_kb)) {
      if(k==271) break;
    }
    else k=0;
    if(k < 0x80 && k) {
    	senddata(k);
    	printf("%d\n", k);
    }
    c = recvdata();
    if(c) {
      newchar(c);
    }
    if(dirty) handle_dirty();
    idle();
  }
  free_buffer();
  return 0;
}