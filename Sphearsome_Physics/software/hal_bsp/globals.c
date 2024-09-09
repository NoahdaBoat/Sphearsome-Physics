#include "globals.h"
#include "jazzplaza.h"

/* global variables - mostly for ISRs */
volatile int buf_index_play; // may need this for background music
volatile unsigned char byte1, byte2, byte3; // Three bytes in a mouse packet
volatile bool button1Down, button2Down, button3Down;
volatile short int mouseX, mouseY; // Mouse acceleration
volatile int byteIndex; // packet order
volatile int packetSize;
volatile bool negX, negY;
volatile int timeout; // used to synchronize with the timer
volatile unsigned char negXMask = (1 << 4);
volatile unsigned char negYMask = (1 << 5);
volatile bool newData;
struct alt_up_dev up_dev;	// struct to hold pointers to open devices
volatile int *audioArrayPosition;
volatile int audioArraySize = sizeof(jazzplaza)/sizeof(int);
volatile int * interval_timer_ptr;
volatile int seq;
volatile int dir = 1;
volatile int delay = 0;
volatile int chaserDelay = 0;
