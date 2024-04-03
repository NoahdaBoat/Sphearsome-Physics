#include "globals.h"
#include "SphearSplash.h"
#include "SphearEnd.h"
#include "jazzplaza2.h"
#include "circles.h"
//#include "time.h"
//#include "sys/alt_alarm.h"

/* Any valus the ISR writes need to be volatile to prevent the optimizing
 * compiler from optimizing/caching values in registers as opposed to
 * reading them fresh */

extern volatile unsigned char byte1, byte2, byte3;
extern volatile int      timeout; // May not need this is timing is solely based on buffer swap
extern struct alt_up_dev up_dev;  // The PS2 mouse device
extern volatile bool button1Down, button2Down;
extern volatile short int mouseX, mouseY;
extern volatile int byteIndex; // packet order
extern volatile int packetSize;
extern volatile bool newData;

/* function prototypes */
void HEX_PS2(unsigned char, unsigned char, unsigned char); // For debugging mouse data stream
void interval_timer_ISR(void *, unsigned int); // May not need this
void PS2_ISR(void *, unsigned int);
void drawSplash(alt_up_video_dma_dev * pixel_dma_dev, int buffer, const short splashScreen[], alt_up_audio_dev * audio_dev, int *audioArrayPosition, const int sample[], int sampleLen);
void binRandS(short a[], short b[], alt_up_audio_dev * audio_dev, int *audioArrayPosition, const int sample[], int sampleLen);
void playBGM(alt_up_audio_dev * audio_dev, int *audioArrayPosition, const int sample[], int sampleLen);
void copyPoint(struct Point* source, struct Point* dest);
void drawCursor(alt_up_video_dma_dev * pixel_dma_dev, int colour, struct Point mouseLoc, int buffer);
void restoreSplash(alt_up_video_dma_dev * pixel_dma_dev, struct Point prev2Mouse, const short background[], int buffer);
void moveMouse(alt_up_ps2_dev * PS2_dev, alt_up_video_dma_dev * pixel_dma_dev, struct Point *currMouse, struct Point *prevMouse, struct Point *prev2Mouse);
int  openPS2Device(alt_up_ps2_dev **PS2_dev_ptr);
int  openAudioDevice(alt_up_audio_dev **audio_dev_ptr);
void resampleColours(alt_up_video_dma_dev * pixel_dma_dev, int *colour_black, int *colour_white, int *colour_blue, int *colour_red, int *colour_green);
int  openPixelDMA(alt_up_video_dma_dev **pixel_dma_dev_ptr);
int  openCharDMA(alt_up_video_dma_dev **char_dma_dev_ptr);
void assignPixelBuffers(alt_up_video_dma_dev *pixel_dma_dev);
void assignCharBuffers(alt_up_video_dma_dev *char_dma_dev);
void initSplash(alt_up_video_dma_dev * pixel_dma_dev, int buffer, const short splashScreen[], alt_up_audio_dev * audio_dev, int *audioArrayPosition, const int sample[], int sampleLen);
void initMouse(alt_up_video_dma_dev * pixel_dma_dev, int colour, struct Point *mouseLoc, struct Point *mousePrevLoc, struct Point *mousePrev2Loc, int buffer);
bool clickedInArea(struct Point mouseClick, struct Point topLeft, struct Point bottomRight);
void displayInstructionsScreen(alt_up_video_dma_dev * pixel_dma_dev, alt_up_video_dma_dev * char_dma_dev);

/*******************************************************************************
 * This program performs the following:
 *	Allows the user to play a fun physics game.
 *	Uses board support to focus on physics and gameplay
 ******************************************************************************/
int main(void) {
    /* PS2 device driver pointer */
    alt_up_ps2_dev * PS2_dev;
    alt_up_ps2_dev **PS2_dev_ptr;

    /* Audio device driver pointer */
    alt_up_audio_dev * audio_dev;
    alt_up_audio_dev **audio_dev_ptr;

    // Video pointers - create variables that point to the
    // pixel buffer and character buffer
    alt_up_video_dma_dev * pixel_dma_dev;
    alt_up_video_dma_dev * char_dma_dev;
    alt_up_video_dma_dev **pixel_dma_dev_ptr;
    alt_up_video_dma_dev **char_dma_dev_ptr;

    /* Volatile pointer for interval timer */
    volatile int * interval_timer_ptr = (int *)INTERVAL_TIMER_BASE; // internal timer base address

    /* Arrays for dissolve effect */
    short randXS[240*320];
	short randYS[240*320];

	/* Audio sample variables */
	int audioArrayPosition = 0;
	int sampleLen = sizeof(jazzplaza)/4;

	/* Initial background state variables */
	bool blackBackground = false;
	bool splashBackground = true;
	bool exitBackground = false;
	bool displayInstructions = false;

	/* Splash screen button locations */
	struct Point level1TopLeft = {54,106};
	struct Point level1BottomRight = {158,129};
	struct Point level2TopLeft = {179,106};
	struct Point level2BottomRight = {281,129};
	struct Point instTopLeft = {55,141};
	struct Point instBottomRight = {158,164};
	struct Point exitTopLeft = {179,139};
	struct Point exitBottomRight = {281,164};
	struct Point levelExitTopLeft = {294,0};
	struct Point levelExitBottomRight = {319,18};

    // Allocate mouse positions on the stack
    struct Point currMouse = {0};
    struct Point prevMouse = {0};
    struct Point prev2Mouse = {0};

    // Start all colours in 24 bit, then convert them based on the device colour depth
    int colour_black	= RGB_24BIT_BLACK;
    int colour_white	= RGB_24BIT_WHITE;
 	int colour_red		= RGB_24BIT_RED;
 	int colour_green	= RGB_24BIT_GREEN;
 	int colour_blue		= RGB_24BIT_BLUE;

    /* initialize PS/2 mouse variables */
    byte1   = 0;
    byte2   = 0;
    byte3   = 0; // Three bytes that make a PS/2 packet
    timeout = 0; // To synchronize with the timer (optional)
    byteIndex = -1;
    button1Down = false;
    button2Down = false;
    packetSize = 3;
    newData = false;

    /* set the interval timer period */
    int counter = 500000; // 1/(100 MHz) x (20000000) = 200 msec
    *(interval_timer_ptr + 0x2) = (counter & 0xFFFF);
    *(interval_timer_ptr + 0x3) = (counter >> 16) & 0xFFFF;

    /* start interval timer, enable its interrupts */
    /* Likely only need this if we don't have enough time for computations */
    //*(interval_timer_ptr + 1) = 0x7; // STOP = 0, START = 1, CONT = 1, ITO = 1

    // open the PS2 port; need double pointer to modify the pointer
    PS2_dev_ptr = &PS2_dev;
    if (openPS2Device(PS2_dev_ptr) != 0) {
    	return -1;
    }

    // We haven't started the timer, but still need to register its ISR
    // Register the PS2 mouse ISR as well
    alt_irq_register(0, (void *)&up_dev, (void *)interval_timer_ISR);
    alt_irq_register(7, (void *)&up_dev, (void *)PS2_ISR);

    // open the audio port; need double pointer to modify the pointer
    audio_dev_ptr = &audio_dev;
    if (openAudioDevice(audio_dev_ptr) != 0) {
    	return -1;
    }

 	// Open the pixel-based direct memory access VGA device
    // Again, need a double pointer
    pixel_dma_dev_ptr = &pixel_dma_dev;
    if (openPixelDMA(pixel_dma_dev_ptr) != 0) {
    	return -1;
    }

    // Assign buffer addresses
    assignPixelBuffers(pixel_dma_dev);

 	// Open the character-based direct memory access VGA device
    // Again, need a double pointer
    char_dma_dev_ptr = &char_dma_dev;
    if (openCharDMA(char_dma_dev_ptr) != 0) {
    	return -1;
    }

    // Assign buffer addresses
    assignCharBuffers(char_dma_dev);

    // Display the initial splash screen
    initSplash(pixel_dma_dev, BACK_BUFFER, SphearSplash, audio_dev, &audioArrayPosition, jazzplaza, sampleLen);

    // Randomize array - could dissolve into the main display
    // This takes some time - good to have it while the splash screen displays
    binRandS(randXS, randYS, audio_dev, &audioArrayPosition, jazzplaza, sampleLen);

    // Resample colours down to 16 bits
    resampleColours(pixel_dma_dev, &colour_black, &colour_white, &colour_blue, &colour_red, &colour_green);

    // Initially draw the mouse in the middle of the display - the cursor is a square
    // void initMouse(pixel_dma_dev, colour_red, currMouse, BACK_BUFFER);
    initMouse(pixel_dma_dev, colour_red, &currMouse, &prevMouse, &prev2Mouse, BACK_BUFFER);

    // For fun and testing, draw a circle there as well
    //alt_up_video_dma_draw_circle(pixel_dma_dev, color_green_x, currMouse.x-3, currMouse.y-3, 9, circle9, BACK_BUFFER);

    // Swap front to back buffers
    //alt_up_video_dma_ctrl_swap_buffers(pixel_dma_dev);

    // for testing circle stuff - should remove
    int cy = 0;
    int cx = 0;
    int cdir = 1;
    bool releaseCircle=false;
    bool grabCircle=false;
    int levelSelected = 0;

    // For testing float speed
    const int numIterations = 50;
    float a;
    float b;
    /* for measuring float performance
    clock_t startAddition;
    clock_t endAddition;
    double timeAddition;
    double timeAdditionAvg;
	clock_t startMultiplication;
    clock_t endMultiplication;
    double timeMultiplication;
    double timeMultiplicationAvg;
    int N=1;

    printf("[TPS]=%lu\n", (alt_u32)alt_ticks_per_second());
    unsigned int TPS = (alt_u32)alt_ticks_per_second();
    */

    while (1) {
        a = 3.14f;
        b = 2.71f;
        //startAddition = clock();
        for (int i = 0; i < numIterations; ++i) {
            a += b;
        }
        /*
        endAddition = clock();
        timeAddition = (double)(endAddition - startAddition) /TPS;
        timeAdditionAvg -= timeAdditionAvg / N;
        timeAdditionAvg += timeAddition / N;
        */
        a = 3.14f;
        b = 2.71f;
        //startMultiplication = clock();
        for (int i = 0; i < numIterations; ++i) {
            a *= b;
        }
        /*
        endMultiplication = clock();
        timeMultiplication = (double)(endMultiplication - startMultiplication) / TPS;
        timeMultiplicationAvg -= timeMultiplicationAvg / N;
        timeMultiplicationAvg += timeMultiplication / N;
        N++;
        */

        // Uncomment this to sync with the timer as opposed to screen refersh
        // while (!timeout)
        //     ; // wait to synchronize with timeout, which is set by the timer ISR

    	// Play background music
    	playBGM(audio_dev, &audioArrayPosition, jazzplaza, sampleLen);

    	// Check if screen swap completed
    	if (alt_up_video_dma_ctrl_check_swap_status(pixel_dma_dev) == 0) {

    		// Clear prev mouse move - black or splash/exit screen
    		if (blackBackground || displayInstructions) {
    			if (blackBackground) {
    				alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);
    				// Draw exit box and text
    				alt_up_video_dma_draw_box(pixel_dma_dev, colour_red, 294, 0, 319, 18, BACK_BUFFER, 1);
    				alt_up_video_dma_draw_string(char_dma_dev, "Exit", 75, 2, BACK_BUFFER);
    			} else {
    				alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);
    			}
    		} else if (splashBackground) {
    			restoreSplash(pixel_dma_dev, prev2Mouse, SphearSplash, BACK_BUFFER);
    		} else if (exitBackground) {
    			// Not created yet
    		}

    		// Should make hex display the score
    		//HEX_PS2(byte1, byte2, byte3); /* Uncomment for PS2 mouse debugging */

    		// testing circle stuff - should remove
            if (releaseCircle) {
				cy += cdir;
				if (cy==0 || cy==pixel_dma_dev->y_resolution-10) {
					cdir *= -1;
				}
				alt_up_video_dma_draw_circle(pixel_dma_dev, colour_green, cx, cy, 9, circle9, BACK_BUFFER);
            }

            if (newData) {
            	newData = false;
            	moveMouse(PS2_dev, pixel_dma_dev, &currMouse, &prevMouse, &prev2Mouse);
            }

            // Mouse button actions
            // Should add a routine to check where the mouse clicked
			if (button1Down) {
				//printf("x=%d, y=%d",currMouse.x, currMouse.y);
				grabCircle = true;
				if (displayInstructions) {
					displayInstructions = false;
					blackBackground = false;
					splashBackground = true;
				    // Display the initial splash screen
					alt_up_video_dma_screen_clear(char_dma_dev, BACK_BUFFER);
				    initSplash(pixel_dma_dev, BACK_BUFFER, SphearSplash, audio_dev, &audioArrayPosition, jazzplaza, sampleLen);
				    drawCursor(pixel_dma_dev, colour_green, currMouse, BACK_BUFFER);
				} else {
					drawCursor(pixel_dma_dev, colour_green, currMouse, BACK_BUFFER);
					if ((splashBackground) && (levelSelected == 0) && (clickedInArea(currMouse, level1TopLeft, level1BottomRight) ||
						clickedInArea(currMouse, level2TopLeft, level2BottomRight))) {
						if (clickedInArea(currMouse, level1TopLeft, level1BottomRight)) {
							levelSelected = 1;
						} else {
							levelSelected = 2;
						}
						// Selected level 1 or 2 dissolve to main play screen
						blackBackground = true;
						splashBackground = false;
						alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);
						for (int i=0; i < 240*320; i++) {
							alt_up_video_dma_draw(pixel_dma_dev, 0, randXS[i], randYS[i], FRONT_BUFFER);
							playBGM(audio_dev, &audioArrayPosition, jazzplaza, sampleLen);
						}
						alt_up_video_dma_draw_box(pixel_dma_dev, colour_red, 294, 0, 319, 18, FRONT_BUFFER, 1);
						alt_up_video_dma_draw_string(char_dma_dev, "Exit", 75, 2, FRONT_BUFFER);
					} else if (clickedInArea(currMouse, instTopLeft, instBottomRight) && (levelSelected == 0)) {
						displayInstructions = true;
						splashBackground = false;
						alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);
						for (int i=0; i < 240*320; i++) {
							alt_up_video_dma_draw(pixel_dma_dev, 0, randXS[i], randYS[i], FRONT_BUFFER);
							playBGM(audio_dev, &audioArrayPosition, jazzplaza, sampleLen);
						}
						displayInstructionsScreen(pixel_dma_dev, char_dma_dev);
					} else if (clickedInArea(currMouse, exitTopLeft, exitBottomRight) && (levelSelected == 0)) {
						initSplash(pixel_dma_dev, BACK_BUFFER, SphearEnd, audio_dev, &audioArrayPosition, jazzplaza, sampleLen);
					    //printf("Number of iterations: %d\n", numIterations);
					    //printf("Time for addition: %.6f seconds\n", timeAdditionAvg);
					    //printf("Time for multiplication: %.6f seconds\n", timeMultiplicationAvg);
						while (1)
							;
					} else if (clickedInArea(currMouse, levelExitTopLeft, levelExitBottomRight) && (levelSelected != 0)) {
						levelSelected = 0;
						blackBackground = false;
						splashBackground = true;
						for (int i=0; i < 240*320; i++) {
							alt_up_video_dma_draw(pixel_dma_dev, 0, randXS[i], randYS[i], FRONT_BUFFER);
							playBGM(audio_dev, &audioArrayPosition, jazzplaza, sampleLen);
						}
					    // Display the initial splash screen
						alt_up_video_dma_screen_clear(char_dma_dev, BACK_BUFFER);
					    initSplash(pixel_dma_dev, BACK_BUFFER, SphearSplash, audio_dev, &audioArrayPosition, jazzplaza, sampleLen);
					    drawCursor(pixel_dma_dev, colour_green, currMouse, BACK_BUFFER);
					}
				}
			} else if (button2Down) {
				releaseCircle = true;
				cx = currMouse.x;
				cy = currMouse.y;
				drawCursor(pixel_dma_dev, colour_white, currMouse, BACK_BUFFER);
			} else {
				if (!grabCircle) {
					//alt_up_video_dma_draw_circle(pixel_dma_dev, color_green_x, (pixel_dma_dev->x_resolution / 2)-3, (pixel_dma_dev->y_resolution / 2)-3, 9, circle9, BACK_BUFFER);
				}
				drawCursor(pixel_dma_dev, colour_red, currMouse, BACK_BUFFER);
			}

			if (blackBackground)
				if (levelSelected == 1)
					alt_up_video_dma_draw_string(char_dma_dev, "Level 1 Selected", 0, 0, BACK_BUFFER);
				else
					alt_up_video_dma_draw_string(char_dma_dev, "Level 2 Selected", 0, 0, BACK_BUFFER);

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

void drawSplash(alt_up_video_dma_dev * pixel_dma_dev, int buffer, const short splashScreen[], alt_up_audio_dev *audio_dev, int *audioArrayPosition, const int sample[], int sampleLen) {
    int index = 0;
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 320; ++x) {
            alt_up_video_dma_draw(pixel_dma_dev, splashScreen[index], x, y, buffer);
            index++;
			playBGM(audio_dev, audioArrayPosition, sample, sampleLen);
        }
    }
}

void binRandS(short a[], short b[], alt_up_audio_dev * audio_dev, int *audioArrayPosition, const int sample[], int sampleLen) {
	int x, y, i, val, tmp;

	for (y=0; y<240;y++)
		for (x=0; x<320;x++) {
			a[y*320+x]=x;
			b[y*320+x]=y;
			playBGM(audio_dev, audioArrayPosition, sample, sampleLen);
		}

	for (i=0;i<320*240;i++) {
		val = (rand() % (320*240 - i)) + i;
		tmp = a[i];
		a[i] = a[val];
		a[val] = tmp;
		tmp = b[i];
		b[i] = b[val];
		b[val] = tmp;
		playBGM(audio_dev, audioArrayPosition, sample, sampleLen);
	}
}

void playBGM(alt_up_audio_dev * audio_dev, int * audioArrayPosition, const int sample[], int sampleLen) {
	// Fill the audio FIFO buffer is there's room
	// It's a mono sound; left and right FIFO buffers should be the same
	// fifospace = alt_up_audio_read_fifo_avail (audio_dev, ALT_UP_AUDIO_RIGHT);
	// Is there room?
	volatile int * audio_ptr = (int *)AUDIO_SUBSYSTEM_AUDIO_BASE;
	int fifospace = *(audio_ptr + 1); // read the audio port fifospace register
	if ((fifospace & 0x00FF0000) > BUF_THRESHOLD) {
		// Some room; keep feeding the FIFO buffers until they are full
		while ((fifospace & 0x00FF0000) && (*audioArrayPosition <= sampleLen)) {
			// write to audio buffers from saved data array
			alt_up_audio_write_fifo (audio_dev, &(sample[*audioArrayPosition]), 1, ALT_UP_AUDIO_RIGHT);
			alt_up_audio_write_fifo (audio_dev, &(sample[*audioArrayPosition]), 1, ALT_UP_AUDIO_LEFT);
			// Increment audio array position and check if we're at the end of the array
			++*audioArrayPosition;
			if (*audioArrayPosition >= sampleLen) {
				// Loop sample
				*audioArrayPosition = 0;
			}
			// Check how much FIFO room is left
			fifospace = *(audio_ptr + 1); // read the audio port fifospace register
		}
	}
}

void copyPoint(struct Point* source, struct Point* dest) {
	memcpy(dest, source, sizeof(struct Point));
}

void drawCursor(alt_up_video_dma_dev * pixel_dma_dev, int colour, struct Point mouseLoc, int buffer) {
	alt_up_video_dma_draw_box(pixel_dma_dev, colour, mouseLoc.x, mouseLoc.y,mouseLoc.x+CURSOR_SIZE, mouseLoc.y+CURSOR_SIZE, buffer, 1);
}

void restoreSplash(alt_up_video_dma_dev * pixel_dma_dev, struct Point prev2Mouse, const short background[], int buffer) {
	int clearlBoundy = prev2Mouse.y-1;
	int clearuBoundy = prev2Mouse.y+CURSOR_SIZE+1;
	int clearlBoundx = prev2Mouse.x-1;
	int clearuBoundx = prev2Mouse.x+CURSOR_SIZE+1;

	if (prev2Mouse.y == 0 || prev2Mouse.y == pixel_dma_dev->y_resolution - CURSOR_SIZE - 1) {
		clearlBoundy++;
		clearuBoundy--;
	}

	if (prev2Mouse.x == 0 || prev2Mouse.x == pixel_dma_dev->x_resolution - CURSOR_SIZE - 1) {
		clearlBoundx++;
		clearuBoundx--;
	}

	for (int y=clearlBoundy; y <= clearuBoundy; y++)
		for (int x=clearlBoundx; x <= clearuBoundx; x++) {
			alt_up_video_dma_draw(pixel_dma_dev, background[x+(y*320)], x, y, buffer);
			//alt_up_video_dma_draw(pixel_dma_dev, splashScreen[x+(y*320)], x, y, BACK_BUFFER);
		}
}

void moveMouse(alt_up_ps2_dev * PS2_dev, alt_up_video_dma_dev * pixel_dma_dev, struct Point *currMouse, struct Point *prevMouse, struct Point *prev2Mouse) {
	// Bounds checking
	if ((currMouse->x + mouseX < 0) || (currMouse->x + mouseX > (pixel_dma_dev->x_resolution - CURSOR_SIZE - 1)))
		if (currMouse->x + mouseX < 0) {
			currMouse->x = 0;
		} else {
			currMouse->x = pixel_dma_dev->x_resolution - CURSOR_SIZE - 1;
		}
		//currMouse->x = prevMouse->x;
	else
		currMouse->x = prevMouse->x + (int)mouseX;

	if ((currMouse->y + mouseY < 0) || (currMouse->y + mouseY > (pixel_dma_dev->y_resolution - CURSOR_SIZE - 1)))
		if (currMouse->y + mouseY < 0) {
			currMouse->y = 0;
		} else {
			currMouse->y = pixel_dma_dev->y_resolution - CURSOR_SIZE - 1;
		}
		//currMouse->y = prevMouse->y;
	else
		currMouse->y = prevMouse->y + (int)mouseY;

    copyPoint(prevMouse, prev2Mouse);
    copyPoint(currMouse, prevMouse);

	// Clear the mouse movement queue
	(void)alt_up_ps2_clear_fifo(PS2_dev);

}

int openPS2Device(alt_up_ps2_dev **PS2_dev_ptr) {
	*PS2_dev_ptr = alt_up_ps2_open_dev("/dev/PS2_Port");
	if (*PS2_dev_ptr == NULL) {
		alt_printf("Error: could not open PS2 device\n");
		return -1;
	} else {
		alt_printf("Opened PS2 device\n");
		up_dev.PS2_dev = *PS2_dev_ptr; // store for use by ISRs
		// Reset the mouse
		(void)alt_up_ps2_write_data_byte(up_dev.PS2_dev, (unsigned char)0xFF);
	}
    (void)alt_up_ps2_write_data_byte(*PS2_dev_ptr, 0xFF); // reset
    (void)alt_up_ps2_write_data_byte(up_dev.PS2_dev, (unsigned char)0xF4);
    alt_up_ps2_enable_read_interrupt(*PS2_dev_ptr); // enable interrupts from PS/2 port

    // Flush the mouse cache
    (void)alt_up_ps2_clear_fifo(*PS2_dev_ptr);

	return 0;
}

int openAudioDevice(alt_up_audio_dev **audio_dev_ptr) {
	// Audio setup
    *audio_dev_ptr = alt_up_audio_open_dev ("/dev/Audio_Subsystem_Audio");
    if ( *audio_dev_ptr == NULL) {
    	alt_printf ("Error: could not open audio device \n");
    	return -1;
    } else {
    	alt_printf ("Opened audio device \n");
    	return 0;
    }
}

void resampleColours(alt_up_video_dma_dev * pixel_dma_dev, int *colour_black, int *colour_white, int *colour_blue, int *colour_red, int *colour_green) {
	if (pixel_dma_dev->data_width == 1) {
    	*colour_black	= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_8BIT(*colour_black);
        *colour_white	= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_8BIT(*colour_white);
        *colour_blue		= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_8BIT(*colour_blue);
        *colour_red		= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_8BIT(*colour_red);
        *colour_green	= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_8BIT(*colour_green);
    } else if (pixel_dma_dev->data_width == 2) {
    	*colour_black	= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_8BIT(*colour_black);
        *colour_white	= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(*colour_white);
        *colour_blue		= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(*colour_blue);
        *colour_red		= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(*colour_red);
        *colour_green	= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(*colour_green);
    }
}

int openPixelDMA(alt_up_video_dma_dev **pixel_dma_dev_ptr) {
	*pixel_dma_dev_ptr = alt_up_video_dma_open_dev("/dev/VGA_Subsystem_VGA_Pixel_DMA");
	if (*pixel_dma_dev_ptr == NULL) {
		alt_printf("Error: could not open VGA's DMA controller device\n");
		return -1;
	} else {
		alt_printf("Opened VGA's DMA controller device\n");
		return 0;
	}
}

int openCharDMA(alt_up_video_dma_dev **char_dma_dev_ptr) {
	*char_dma_dev_ptr = alt_up_video_dma_open_dev("/dev/VGA_Subsystem_Char_Buf_Subsystem_Char_Buf_DMA");
	if (*char_dma_dev_ptr == NULL) {
		alt_printf("Error: could not open character buffer's DMA controller device\n");
		return -1;
	} else {
		alt_printf("Opened character buffer's DMA controller device\n");
		return 0;
	}
}

void assignPixelBuffers(alt_up_video_dma_dev *pixel_dma_dev) {
    // Should really set the address in the system,
    // this way we are guaranteed to place the buffers where we want
    alt_up_video_dma_ctrl_set_bb_addr(pixel_dma_dev, FRONT_BUFFER_ADDRESS);

    // Clear the current back buffer (front buffer address) before the swap
    alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);

    // Swap and wait for the swap to complete
    alt_up_video_dma_ctrl_swap_buffers(pixel_dma_dev);
    while (alt_up_video_dma_ctrl_check_swap_status(pixel_dma_dev))
      ;

    // Assign the other buffer address - again, should be in the system
    alt_up_video_dma_ctrl_set_bb_addr(pixel_dma_dev, BACK_BUFFER_ADDRESS);

    // Keep the back buffer hidden until the splash screen is drawn
}

void assignCharBuffers(alt_up_video_dma_dev *char_dma_dev) {
    // The front and back buffer are the same for text
    // They don't have to be, but currently are since they are alpha'd in front of the pixel buffer
    // And the text isn't animated

    alt_up_video_dma_screen_clear(char_dma_dev, BACK_BUFFER);
    alt_up_video_dma_ctrl_set_bb_addr(
        char_dma_dev, VGA_SUBSYSTEM_CHAR_BUF_SUBSYSTEM_ONCHIP_SRAM_BASE);
    alt_up_video_dma_ctrl_swap_buffers(char_dma_dev);
    while (alt_up_video_dma_ctrl_check_swap_status(char_dma_dev))
        ;

    alt_up_video_dma_ctrl_set_bb_addr(
        char_dma_dev, VGA_SUBSYSTEM_CHAR_BUF_SUBSYSTEM_ONCHIP_SRAM_BASE);
    alt_up_video_dma_screen_clear(char_dma_dev, BACK_BUFFER);
}

void initSplash(alt_up_video_dma_dev * pixel_dma_dev, int buffer, const short splashScreen[], alt_up_audio_dev *audio_dev, int *audioArrayPosition, const int sample[], int sampleLen) {

	drawSplash(pixel_dma_dev, buffer, splashScreen, audio_dev, audioArrayPosition, sample, sampleLen);

	// Randomize array - could dissolve into the main display
	//binRandS(randXS,randYS);

	// Swap to show the splash screen
	alt_up_video_dma_ctrl_swap_buffers(pixel_dma_dev);
		while (alt_up_video_dma_ctrl_check_swap_status(pixel_dma_dev))
		  ;

	drawSplash(pixel_dma_dev, buffer, splashScreen, audio_dev, audioArrayPosition, sample, sampleLen);
}

void initMouse(alt_up_video_dma_dev * pixel_dma_dev, int colour, struct Point *mouseLoc, struct Point *mousePrevLoc, struct Point *mousePrev2Loc, int buffer) {
    mouseLoc->x = pixel_dma_dev->x_resolution / 2;
    mouseLoc->y = pixel_dma_dev->y_resolution / 2;

    // Draw the initial mouse position
    drawCursor(pixel_dma_dev, colour, *mouseLoc, buffer);
    copyPoint(mouseLoc, mousePrevLoc);
    copyPoint(mouseLoc, mousePrev2Loc);
}

bool clickedInArea(struct Point mouseClick, struct Point topLeft, struct Point bottomRight) {

	if ((mouseClick.x >= topLeft.x) &&
		(mouseClick.x <= bottomRight.x) &&
		(mouseClick.y >= topLeft.y) &&
		(mouseClick.y <= bottomRight.y))
		return true;
	else
		return false;
}

void displayInstructionsScreen(alt_up_video_dma_dev * pixel_dma_dev, alt_up_video_dma_dev * char_dma_dev) {
	// Store instruction text in globals?
	alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);
    alt_up_video_dma_ctrl_swap_buffers(pixel_dma_dev);

    while (alt_up_video_dma_ctrl_check_swap_status(pixel_dma_dev))
      ;
	alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);

	alt_up_video_dma_draw_string(char_dma_dev, "Instructions", 0, 0, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "Click anywhere to return", 0, 1, BACK_BUFFER);
}
