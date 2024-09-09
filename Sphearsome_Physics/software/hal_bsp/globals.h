#include "altera_up_avalon_video_dma_controller.h"
#include "altera_up_avalon_video_rgb_resampler.h"
#include "altera_up_avalon_audio.h"
#include "altera_up_avalon_ps2.h"
#include "sys/alt_stdio.h"
#include "sys/alt_irq.h"
#include "system.h"
#include "stdbool.h"
#include "altera_avalon_pio_regs.h"
#include "string.h"
#include "math.h"
#include "stdlib.h"

#ifndef HEX5_HEX4_BASE
#define HEX5_HEX4_BASE HEX7_HEX4_BASE
#endif

#define RGB_24BIT_BLACK			0x000000
#define RGB_24BIT_WHITE			0xFFFFFF
#define RGB_24BIT_RED			0xFF0000
#define RGB_24BIT_GREEN			0x00FF00
//#define RGB_24BIT_BLUE			0x0000FF
#define RGB_24BIT_BLUE			0x0071C5

#define BUF_THRESHOLD 96

#define FRONT_BUFFER 0
#define BACK_BUFFER 1

#define FRONT_BUFFER_ADDRESS 0x08000000
#define BACK_BUFFER_ADDRESS  0x08200000

#define CURSOR_SIZE 2

#define NUM_SQUARES 8
#define BOX_SIZE 5
#define VGA_SUBSYSTEM_VGA_PIXEL_DMA_BASE 0xff203020
#define MAX_CIRCLES 2
#define GRAVITY_CONST 10

#define box_x1 280
#define box_x2 319
#define box_y1 200
#define box_y2 240

#define legal_x1 0
#define legal_x2 180
#define legal_y1 10
#define legal_y2 30

#define Y_RAND_MAX 240
#define GEN_RAD 9

/*
 * This structure holds a pointer to each of the open devices
*/
struct alt_up_dev {
	alt_up_ps2_dev *PS2_dev;
	alt_up_audio_dev *audio_dev;
};

// Holds mouse positions
struct Point {
	int x;
		int y;
};
