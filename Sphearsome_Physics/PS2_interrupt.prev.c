#include "globals.h"
#include "testing2.h"
#include "splash1.h"
#include "SphearSplash.h"
#include "jazzplaza2.h"

#define FRONT_BUFFER 0
#define BACK_BUFFER 1

/* Any valus the ISR writes need to be volatile to prevent the optimizing
 * compiler from optimizing/caching values in registers as opposed to
 * reading them fresh */

extern volatile unsigned char byte1, byte2, byte3;
extern volatile int      timeout; // May not need this is timing is solely based on buffer swap
extern struct alt_up_dev up_dev;  // The PS2 mouse device
extern volatile bool button1Down, button2Down, button3Down;
extern volatile short int mouseX, mouseY;
extern volatile int byteIndex; // packet order
extern volatile int packetSize;
extern volatile bool newData;

/* function prototypes */
void HEX_PS2(unsigned char, unsigned char, unsigned char); // For debugging mouse data stream
void interval_timer_ISR(void *, unsigned int); // May not need this
void PS2_ISR(void *, unsigned int);
void draw_big_A(alt_up_video_dma_dev *, int color, int buffer); // To delete
void drawSplash(alt_up_video_dma_dev * pixel_dma_dev, int buffer, short splashScreen[]);
void drawMouseBG(alt_up_video_dma_dev * pixel_dma_dev, int buffer,int bg[4], int mousex, int mousey);
void binRandS(short a[], short b[]);

/*******************************************************************************
 * This program performs the following:
 *	Allows the user to play a fun physics game.
 *	Uses board support to focus on physics and gameplay
 ******************************************************************************/
int main(void) {
    /* PS2 device driver pointer */
    alt_up_ps2_dev * PS2_dev;

    /* Audio device driver pointer */
    alt_up_audio_dev * audio_dev;

    /* Audio left/right FIFO buffer variables */
    unsigned int l_buf;
    unsigned int r_buf;

	short randXS[240*320];
	short randYS[240*320];

    /* Volatile pointer for interval timer - may not need this */
    volatile int * interval_timer_ptr = (int *)INTERVAL_TIMER_BASE; // internal timer base address

    /* initialize some variables */
    byte1   = 0;
    byte2   = 0;
    byte3   = 0; // used to hold PS/2 data
    timeout = 0; // synchronize with the timer
    byteIndex = -1;
    button1Down = false;
    button2Down = false;
    mouseX = 0;
    mouseY = 0;
    packetSize = 3;
    newData = false;

    /* set the interval timer period for scrolling the HEX displays */
    int counter = 500000; // 1/(100 MHz) x (20000000) = 200 msec
    *(interval_timer_ptr + 0x2) = (counter & 0xFFFF);
    *(interval_timer_ptr + 0x3) = (counter >> 16) & 0xFFFF;

    /* start interval timer, enable its interrupts */
    //*(interval_timer_ptr + 1) = 0x7; // STOP = 0, START = 1, CONT = 1, ITO = 1

    // open the PS2 port
    PS2_dev = alt_up_ps2_open_dev("/dev/PS2_Port");
    if (PS2_dev == NULL) {
        alt_printf("Error: could not open PS2 device\n");
        return -1;
    } else {
        alt_printf("Opened PS2 device\n");
        up_dev.PS2_dev = PS2_dev; // store for use by ISRs
        // Reset the mouse
        (void)alt_up_ps2_write_data_byte(up_dev.PS2_dev, (unsigned char)0xFF);
    }
    (void)alt_up_ps2_write_data_byte(PS2_dev, 0xFF); // reset
    (void)alt_up_ps2_write_data_byte(up_dev.PS2_dev, (unsigned char)0xF4);
    alt_up_ps2_enable_read_interrupt(
        PS2_dev); // enable interrupts from PS/2 port

    // Flush the mouse cache
    (void)alt_up_ps2_clear_fifo(PS2_dev);

    /* Comment from sample program:
     * use the HAL facility for registering interrupt service routines. */

    /* Note: we are passing a pointer to up_dev to each ISR (using the context
     * argument) as a way of giving the ISR a pointer to every open device. This
     * is useful because some of the ISRs need to access more than just one
     * device (e.g. the pushbutton ISR accesses both the pushbutton device and
     * the audio device) */

    alt_irq_register(0, (void *)&up_dev, (void *)interval_timer_ISR);
    alt_irq_register(7, (void *)&up_dev, (void *)PS2_ISR);

    // Audio setup
    audio_dev = alt_up_audio_open_dev ("/dev/Audio_Subsystem_Audio");
    if ( audio_dev == NULL)
    	alt_printf ("Error: could not open audio device \n");
    else
    	alt_printf ("Opened audio device \n");

    // Video graphics setup
    alt_up_video_dma_dev * pixel_dma_dev;
    alt_up_video_dma_dev * char_dma_dev;

    int x1, y1, x2, y2, deltax_1, deltax_2, deltay_1, deltay_2; //, delay = 0;
    int cursorSize = 2;
    unsigned int mousex, mousey, prevmousex, prevmousey = 0;
    unsigned int prevmousex2, prevmousey2 = 0;
    unsigned char prevbyte1 = '0';
    unsigned char prevbyte2 = '0';
    unsigned char prevbyte3 = '0';

	// Demonstrates the 80x60 character buffer
	char text_top_row[40]    = "Intel FPGA\0";
    char text_bottom_row[40] = "Computer Systems\0";

    // Circles need to look like circles and be performant
    // Define all circle sizes required like this - should be in an include
    int circle9[9][9] = {{0,0,1,1,1,1,1,0,0},
    					 {0,1,1,1,1,1,1,1,0},
						 {1,1,1,1,1,1,1,1,1},
						 {1,1,1,1,1,1,1,1,1},
						 {1,1,1,1,1,1,1,1,1},
						 {1,1,1,1,1,1,1,1,1},
						 {1,1,1,1,1,1,1,1,1},
						 {0,1,1,1,1,1,1,1,0},
						 {0,0,1,1,1,1,1,0,0}};

    // Start all colours this way, then convert them based on the colour depth
    int color_large_A			= RGB_24BIT_WHITE;
    int color_text_background	= RGB_24BIT_INTEL_BLUE;
 	int color_red_box			= RGB_24BIT_RED;
 	int color_green_x			= RGB_24BIT_GREEN;

 	// Open the pixel-based direct memory access device
    pixel_dma_dev = alt_up_video_dma_open_dev("/dev/VGA_Subsystem_VGA_Pixel_DMA");
    if (pixel_dma_dev == NULL)
        alt_printf("Error: could not open VGA's DMA controller device\n");
    else
        alt_printf("Opened VGA's DMA controller device\n");

    // Should really set the address in the system, but was seeing what worked best buffer-wise
    alt_up_video_dma_ctrl_set_bb_addr(pixel_dma_dev, 0x08000000);

    // Blank out the back buffer
    //alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);
    drawSplash(pixel_dma_dev, BACK_BUFFER, SphearSplash);
    // Swap and wait for the swap to complete
    alt_up_video_dma_ctrl_swap_buffers(pixel_dma_dev);
    while (alt_up_video_dma_ctrl_check_swap_status(pixel_dma_dev))
      ;

    // Assign the other buffer address - again, should be in the system
    alt_up_video_dma_ctrl_set_bb_addr(pixel_dma_dev, 0x08200000);

    // Clear it
    //alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);
    drawSplash(pixel_dma_dev, BACK_BUFFER, SphearSplash);
    // Do roughly the same for the character array; the front and back buffer are the same for text
    // They don't have to be, but currently are since they are alpha'd in front of the pixel buffer
    // And the text isn't animated
    char_dma_dev = alt_up_video_dma_open_dev(
        "/dev/VGA_Subsystem_Char_Buf_Subsystem_Char_Buf_DMA");
    if (char_dma_dev == NULL) {
        alt_printf(
            "Error: could not open character buffer's DMA controller device\n");
        //return -1;
    } else
        alt_printf("Opened character buffer's DMA controller device\n");

    alt_up_video_dma_ctrl_set_bb_addr(
        char_dma_dev, VGA_SUBSYSTEM_CHAR_BUF_SUBSYSTEM_ONCHIP_SRAM_BASE);
    alt_up_video_dma_ctrl_swap_buffers(char_dma_dev);
    while (alt_up_video_dma_ctrl_check_swap_status(char_dma_dev))
        ;
    alt_up_video_dma_ctrl_set_bb_addr(
        char_dma_dev, VGA_SUBSYSTEM_CHAR_BUF_SUBSYSTEM_ONCHIP_SRAM_BASE);

    // Resample colours down to 16 bits
    if (pixel_dma_dev->data_width == 1) {
        color_large_A			= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_8BIT(color_large_A);
        alt_printf("%d\n", color_large_A);
        color_text_background	= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_8BIT(color_text_background);
        color_red_box			= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_8BIT(color_red_box);
        color_green_x			= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_8BIT(color_green_x);
    } else if (pixel_dma_dev->data_width == 2) {
        color_large_A			= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(color_large_A);
        color_text_background	= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(color_text_background);
        color_red_box			= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(color_red_box);
        color_green_x			= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(color_green_x);
    }
    // Clear the front character buffer
    alt_up_video_dma_screen_clear(char_dma_dev, FRONT_BUFFER);

    // Draw a bunch of stuff on the back buffer - likely will delete
    /*
    int char_mid_x = char_dma_dev->x_resolution / 2;
    int char_mid_y = char_dma_dev->y_resolution / 2;
    alt_up_video_dma_draw_string(char_dma_dev, text_top_row, char_mid_x - 5,
							  	  char_mid_y - 1, BACK_BUFFER);
    alt_up_video_dma_draw_string(char_dma_dev, text_bottom_row, char_mid_x - 8,
							  	  char_mid_y, BACK_BUFFER);

  	int x_diff_factor = pixel_dma_dev->x_resolution / char_dma_dev->x_resolution;
   	int y_diff_factor = pixel_dma_dev->y_resolution / char_dma_dev->y_resolution;
    alt_up_video_dma_draw_box(
         pixel_dma_dev, color_text_background,
   		(char_mid_x - 9) * x_diff_factor, (char_mid_y - 2) * y_diff_factor,
        (char_mid_x + 9) * x_diff_factor - 1, (char_mid_y + 2) * y_diff_factor - 1,
  		BACK_BUFFER, 1);

    draw_big_A(pixel_dma_dev, color_large_A, BACK_BUFFER);


    int mx = pixel_dma_dev->x_resolution / 160;
    int my = pixel_dma_dev->y_resolution / 120;

    x1 = 10 * mx;
    y1 = 10 * my;
    x2 = 25 * mx;
    y2 = 25 * my;
    alt_up_video_dma_draw_box(pixel_dma_dev, color_red_box, x1, y1, x2, y2, 0, BACK_BUFFER);
    alt_up_video_dma_draw_line(pixel_dma_dev, color_green_x, x1, y1, x2, y2, BACK_BUFFER);
    alt_up_video_dma_draw_line(pixel_dma_dev, color_green_x, x1, y2, x2, y1, BACK_BUFFER);
	*/

    // Initially draw the mouse in the middle of the display - the mouse is a square
    mousex = pixel_dma_dev->x_resolution / 2;
    mousey = pixel_dma_dev->y_resolution / 2;
    alt_up_video_dma_draw_box(pixel_dma_dev, color_red_box, mousex, mousey, mousex+cursorSize, mousey+cursorSize, 0, BACK_BUFFER);
    prevmousex = mousex;
    prevmousey = mousey;
    prevmousex2 = mousex;
    prevmousey2 = mousey;

    // For fun and testing, draw a circle there as well
    //alt_up_video_dma_draw_circle(pixel_dma_dev, color_green_x, mousex-3, mousey-3, 9, circle9, BACK_BUFFER);

    // Swap front to back buffers
    alt_up_video_dma_ctrl_swap_buffers(pixel_dma_dev);

    deltax_1 = deltax_2 = deltay_1 = deltay_2 = 1;

   /* int dissolveSpeed = 7;
    srand(time(NULL));
    // dissolve fade effect array - 30 passes
    short dissolve[pixel_dma_dev->y_resolution][pixel_dma_dev->x_resolution];
    for (int y = 0; y < pixel_dma_dev->y_resolution; y++)
    	for (int x = 0; x < pixel_dma_dev->x_resolution; x++)
    		dissolve[y][x]= rand() % dissolveSpeed + 1;
	*/

	binRandS(randXS,randYS);

    // for testing
    int cy = 0;
    int cx = 0;
    int cdir = 1;
    bool releaseCircle=false;
    bool grabCircle=false;
    int audioArrayPosition = 0;
    int sampleLen = sizeof(jazzplaza)/4;
    int lb;
    int rb;
    int fifospace;
    volatile int * audio_ptr   = (int *)AUDIO_SUBSYSTEM_AUDIO_BASE;
    int buffer_index = 0;
    unsigned int writeSpace;
    int          num_written;
    unsigned short tempBuffer[320][240] = {0};
    //int bgmIndex = 0;
    unsigned int offset = 0;
    int sy = 0;
    int clearlBoundy, clearuBoundy, clearlBoundx, clearuBoundx = 0;
	int dDelay = 15;

    while (1) {
    	// Uncomment this to sync with the timer as opposed to screen refersh
        // while (!timeout)
        //     ; // wait to synchronize with timeout, which is set by the timer ISR

    	// Fill the audio FIFO buffer is there's room
    	// It's a mono sound; left and right FIFO buffers should be the same
    	//fifospace = alt_up_audio_read_fifo_avail (audio_dev, ALT_UP_AUDIO_RIGHT);
    	// Is there room?
    	fifospace = *(audio_ptr + 1); // read the audio port fifospace register
    	if ((fifospace & 0x00FF0000) > BUF_THRESHOLD) {
    		// Some room; keep feeding the FIFO buffers until they are full
    		while ((fifospace & 0x00FF0000) && (audioArrayPosition <= sampleLen)) {
				// write to audio buffers from saved data array
				//*(audio_ptr + 2) = jazzplaza[buffer_index];
				//*(audio_ptr + 3) = jazzplaza[buffer_index];
				alt_up_audio_write_fifo (audio_dev, &(jazzplaza[audioArrayPosition]), 1, ALT_UP_AUDIO_RIGHT);
				alt_up_audio_write_fifo (audio_dev, &(jazzplaza[audioArrayPosition]), 1, ALT_UP_AUDIO_LEFT);
				// Increment audio array position and check if we're at the end of the array
    			++audioArrayPosition;
				if (audioArrayPosition >= sampleLen) {
					// Loop sample
					audioArrayPosition = 0;
				}
				// Check how much FIFO room is left
				fifospace = *(audio_ptr + 1); // read the audio port fifospace register
    		}
    	}

    	if (alt_up_video_dma_ctrl_check_swap_status(pixel_dma_dev) == 0) {

    		// Start with a clean back buffer
    		//(void)alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);

    		//HEX_PS2(byte1, byte2, byte3); /* Uncomment for PS2 mouse debugging */
    		// testing circle stuff
            if (releaseCircle) {
				cy += cdir;
				if (cy==0 || cy==pixel_dma_dev->y_resolution-10) {
					cdir *= -1;
				}
				alt_up_video_dma_draw_circle(pixel_dma_dev, color_green_x, cx, cy, 9, circle9, BACK_BUFFER);
            }

            // For backgrounds, restore the buffer area where the mouse was a buffer exchange ago */
            clearlBoundy = prevmousey2-1;
            clearuBoundy = prevmousey2+cursorSize+1;
            if (prevmousey2 == 0 || prevmousey2 == pixel_dma_dev->y_resolution - cursorSize - 1) {
				clearlBoundy++;
				clearuBoundy--;
			}
            clearlBoundx = prevmousex2-1;
            clearuBoundx = prevmousex2+cursorSize+1;
            if (prevmousex2 == 0 || prevmousex2 == pixel_dma_dev->x_resolution - cursorSize - 1) {
				clearlBoundx++;
				clearuBoundx--;
			}
            for (int y=clearlBoundy; y <= clearuBoundy; y++)
				for (int x=clearlBoundx; x <= clearuBoundx; x++) {
					alt_up_video_dma_draw(pixel_dma_dev, SphearSplash[x+(y*320)], x, y, BACK_BUFFER);
					//alt_up_video_dma_draw(pixel_dma_dev, splashScreen[x+(y*320)], x, y, BACK_BUFFER);
				}

            // If the mouse didn't move or buttons click, then don't process mouse stuff
            //if ((byte1 != prevbyte1 || byte2 != prevbyte2 || byte3 != prevbyte3)) {
            if (newData) {
            	newData = false;
            	//alt_up_video_dma_draw_box(pixel_dma_dev, RGB_24BIT_BLACK, prevmousex, prevmousey, prevmousex+cursorSize, prevmousey+cursorSize, BACK_BUFFER, 1);

            	// Bounds checking
				if ((mousex + mouseX < 0) || (mousex + mouseX > (pixel_dma_dev->x_resolution - cursorSize - 1)))
					mousex = prevmousex;
				else
					mousex = prevmousex + (int)mouseX;
				if ((mousey + -mouseY <= 0) || (mousey + mouseY > (pixel_dma_dev->y_resolution - cursorSize - 1)))
					mousey = prevmousey;
				else
					mousey = prevmousey + (int)mouseY;

				prevmousex2 = prevmousex;
			    prevmousey2 = prevmousey;
			    prevmousex = mousex;
				prevmousey = mousey;

				// Clear the movement queue
				(void)alt_up_ps2_clear_fifo(PS2_dev);
            }
			prevbyte1 = byte1;
			prevbyte2 = byte2;
			prevbyte3 = byte3;

			// Can add this delay to slow down animation to 30 FPS
            //delay = 1 - delay;
            //if (delay == 0) {
				//(void)alt_up_video_dma_screen_clear(pixel_dma_dev, 0);
              //  alt_up_video_dma_draw_box(pixel_dma_dev, 0, x1, y1, x2, y2, BACK_BUFFER, 0);
              //  alt_up_video_dma_draw_line(pixel_dma_dev, 0, x1, y1, x2, y2, BACK_BUFFER);
              //  alt_up_video_dma_draw_line(pixel_dma_dev, 0, x1, y2, x2, y1, BACK_BUFFER);

			// move the rectangle
			/*
			x1 = x1 + deltax_1;
			x2 = x2 + deltax_2;
			y1 = y1 + deltay_1;
			y2 = y2 + deltay_2;
			if ((deltax_1 > 0) &&
				(x1 >= pixel_dma_dev->x_resolution - 1)) {
				x1 = pixel_dma_dev->x_resolution - 1;
				deltax_1 = -deltax_1;
			} else if ((deltax_1 < 0) && (x1 <= 0)) {
				x1       = 0;
				deltax_1 = -deltax_1;
			}
			if ((deltax_2 > 0) &&
				(x2 >= pixel_dma_dev->x_resolution - 1)) {
				x2 = pixel_dma_dev->x_resolution - 1;
				deltax_2 = -deltax_2;
			} else if ((deltax_2 < 0) && (x2 <= 0)) {
				x2       = 0;
				deltax_2 = -deltax_2;
			}
			if ((deltay_1 > 0) &&
				(y1 >= pixel_dma_dev->y_resolution - 1)) {
				y1 = pixel_dma_dev->y_resolution - 1;
				deltay_1 = -deltay_1;
			} else if ((deltay_1 < 0) && (y1 <= 0)) {
				y1       = 0;
				deltay_1 = -deltay_1;
			}
			if ((deltay_2 > 0) &&
				(y2 >= pixel_dma_dev->y_resolution - 1)) {
				y2 = pixel_dma_dev->y_resolution - 1;
				deltay_2 = -deltay_2;
			} else if ((deltay_2 < 0) && (y2 <= 0)) {
				y2       = 0;
				deltay_2 = -deltay_2;
			}
			*/
			//background - way too slow!
			//drawSplash(pixel_dma_dev, BACK_BUFFER);

			// redraw Rectangle with diagonal lines
			/*
			alt_up_video_dma_draw_box(pixel_dma_dev, color_red_box, x1, y1, x2, y2, BACK_BUFFER, 0);
			alt_up_video_dma_draw_line(pixel_dma_dev, color_green_x, x1, y1, x2, y2, BACK_BUFFER);
			alt_up_video_dma_draw_line(pixel_dma_dev, color_green_x, x1, y2, x2, y1, BACK_BUFFER);

			// redraw the box in the foreground
			alt_up_video_dma_draw_box(
				pixel_dma_dev, color_text_background,
				(char_mid_x - 9) * x_diff_factor, (char_mid_y - 2) * y_diff_factor,
				(char_mid_x + 9) * x_diff_factor - 1, (char_mid_y + 2) * y_diff_factor - 1,
				BACK_BUFFER, 1);

			//alt_up_video_dma_draw_string(char_dma_dev, "Mouse", char_mid_x - 5,
			//                             char_mid_y - 1, BACK_BUFFER);
			//alt_up_video_dma_draw_string(char_dma_dev, "Working!", char_mid_x - 8,
			//                             char_mid_y, BACK_BUFFER);

			draw_big_A(pixel_dma_dev, color_large_A, BACK_BUFFER);
			*/
			if (button1Down) {
				grabCircle = true;
				(void)alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);
				 for (int i=0; i < 240*320; i++) {
					 	 alt_up_video_dma_draw(pixel_dma_dev, 0, randXS[i], randYS[i], FRONT_BUFFER);
						fifospace = *(audio_ptr + 1); // read the audio port fifospace register
						if ((fifospace & 0x00FF0000) > BUF_THRESHOLD) {
							// Some room; keep feeding the FIFO buffers until they are full
							while ((fifospace & 0x00FF0000) && (audioArrayPosition <= sampleLen)) {
								// write to audio buffers from saved data array
								//*(audio_ptr + 2) = jazzplaza[buffer_index];
								//*(audio_ptr + 3) = jazzplaza[buffer_index];
								alt_up_audio_write_fifo (audio_dev, &(jazzplaza[audioArrayPosition]), 1, ALT_UP_AUDIO_RIGHT);
								alt_up_audio_write_fifo (audio_dev, &(jazzplaza[audioArrayPosition]), 1, ALT_UP_AUDIO_LEFT);
								// Increment audio array position and check if we're at the end of the array
								++audioArrayPosition;
								if (audioArrayPosition >= sampleLen) {
									// Loop sample
									audioArrayPosition = 0;
								}
								// Check how much FIFO room is left
								fifospace = *(audio_ptr + 1); // read the audio port fifospace register
							}
						}

					 	 for (int i=0;i<dDelay;i++) {
							// Screen dissolve delay
						}
				}

				alt_up_video_dma_draw_box(pixel_dma_dev, color_green_x, mousex, mousey, mousex+cursorSize, mousey+cursorSize, BACK_BUFFER, 1);
				if (!releaseCircle) {
					//alt_up_video_dma_draw_circle(pixel_dma_dev, color_green_x, mousex-3, mousey-3, 9, circle9, BACK_BUFFER);
				}
			} else if (button2Down) {
				releaseCircle = true;
				cx = mousex;
				cy = mousey;
				alt_up_video_dma_draw_box(pixel_dma_dev, color_large_A, mousex, mousey, mousex+cursorSize, mousey+cursorSize, BACK_BUFFER, 1);
			} else {
				if (!grabCircle) {
					//alt_up_video_dma_draw_circle(pixel_dma_dev, color_green_x, (pixel_dma_dev->x_resolution / 2)-3, (pixel_dma_dev->y_resolution / 2)-3, 9, circle9, BACK_BUFFER);
				}
				alt_up_video_dma_draw_box(pixel_dma_dev, color_red_box, mousex, mousey, mousex+cursorSize, mousey+cursorSize, BACK_BUFFER, 1);
			}

            //} // for animation delay

            /* Swap buffers and wait at the top */
            alt_up_video_dma_ctrl_swap_buffers(pixel_dma_dev);
        }
    }
}

/*******************************************************************************
 * Subroutine to show a string of HEX data on the HEX displays
 * Note that we are using pointer accesses for the HEX displays parallel port.
 * We could also use the HAL functions for these ports instead
 ******************************************************************************/
void HEX_PS2(volatile unsigned char b1, unsigned char b2, unsigned char b3) {
    volatile int * HEX3_HEX0_ptr = (int *)HEX3_HEX0_BASE;
    volatile int * HEX5_HEX4_ptr = (int *)HEX5_HEX4_BASE;

    /* SEVEN_SEGMENT_DECODE_TABLE gives the on/off settings for all segments in
     * a single 7-seg display, for the hex digits 0 - F */
    unsigned char seven_seg_decode_table[] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
        0x7F, 0x67, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};
    unsigned char hex_segs[] = {0, 0, 0, 0, 0, 0, 0, 0};
    unsigned int  shift_buffer, nibble;
    unsigned char code;
    int           i;

    shift_buffer = (b1 << 16) | (b2 << 8) | b3;
    for (i = 0; i < 6; ++i) {
        nibble = shift_buffer & 0x0000000F; // character is in rightmost nibble
        code   = seven_seg_decode_table[nibble];
        hex_segs[i]  = code;
        shift_buffer = shift_buffer >> 4;
    }
    /* drive the hex displays */
    *(HEX3_HEX0_ptr) = *(int *)(hex_segs);
    *(HEX5_HEX4_ptr) = *(int *)(hex_segs + 4);
}

/* draws a big letter A on the screen */
void draw_big_A(alt_up_video_dma_dev * pixel_dma_dev, int color, int buffer) {
	// Calculate multiplication factor
	int mx = pixel_dma_dev->x_resolution / 160;
	int my = pixel_dma_dev->y_resolution / 120;

   	alt_up_video_dma_draw_line(pixel_dma_dev, color,  5 * mx, 44 * my, 22 * mx,  5 * my, buffer);
    alt_up_video_dma_draw_line(pixel_dma_dev, color, 22 * mx,  5 * my, 36 * mx,  5 * my, buffer);
    alt_up_video_dma_draw_line(pixel_dma_dev, color, 36 * mx,  5 * my, 53 * mx, 44 * my, buffer);
    alt_up_video_dma_draw_line(pixel_dma_dev, color, 53 * mx, 44 * my, 40 * mx, 44 * my, buffer);
    alt_up_video_dma_draw_line(pixel_dma_dev, color, 40 * mx, 44 * my, 37 * mx, 38 * my, buffer);
    alt_up_video_dma_draw_line(pixel_dma_dev, color, 37 * mx, 38 * my, 20 * mx, 38 * my, buffer);
    alt_up_video_dma_draw_line(pixel_dma_dev, color, 20 * mx, 38 * my, 17 * mx, 44 * my, buffer);
    alt_up_video_dma_draw_line(pixel_dma_dev, color, 17 * mx, 44 * my,  5 * mx, 44 * my, buffer);
    alt_up_video_dma_draw_line(pixel_dma_dev, color, 23 * mx, 30 * my, 29 * mx, 16 * my, buffer);
    alt_up_video_dma_draw_line(pixel_dma_dev, color, 29 * mx, 16 * my, 34 * mx, 30 * my, buffer);
    alt_up_video_dma_draw_line(pixel_dma_dev, color, 34 * mx, 30 * my, 23 * mx, 30 * my, buffer);
}

void drawSplash(alt_up_video_dma_dev * pixel_dma_dev, int buffer, short splashScreen[]) {
    int index = 0;
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 320; ++x) {
            alt_up_video_dma_draw(pixel_dma_dev, splashScreen[index], x, y, buffer);
            index++;
        }
    }
}

void drawMouseBG(alt_up_video_dma_dev * pixel_dma_dev, int buffer,int bgm[4], int mousex, int mousey) {
	int xcm = pixel_dma_dev->x_coord_mask;
    int ycm = pixel_dma_dev->y_coord_mask;
    int xco = pixel_dma_dev->x_coord_offset;
    int yco = pixel_dma_dev->y_coord_offset;
    int index = 0;
}

void binRandS(short a[], short b[]) {
	int x, y, i, val, tmp;

	for (y=0; y<240;y++)
		for (x=0; x<320;x++) {
			a[y*320+x]=x;
			b[y*320+x]=y;
		}

	for (i=0;i<320*240;i++) {
		val = (rand() % (320*240 - i)) + i;
		tmp = a[i];
		a[i] = a[val];
		a[val] = tmp;
		tmp = b[i];
		b[i] = b[val];
		b[val] = tmp;
	}
}
