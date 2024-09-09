#include "globals.h"
#include "SphearSplash.h"
#include "SphearEnd.h"
#include "circles.h"

//#include "time.h"
//#include "sys/alt_alarm.h"

/* Any valus the ISR writes need to be volatile to prevent the optimizing
 * compiler from optimizing/caching values in registers as opposed to
 * reading them fresh */

extern volatile unsigned char byte1, byte2, byte3;
extern volatile int      timeout; // May not need this is timing is solely based on buffer swap
extern struct alt_up_dev up_dev;  // PS2 and audio devices
extern volatile int *audioArrayPosition;
extern volatile int audioArraySize;
extern volatile bool button1Down, button2Down;
extern volatile short int mouseX, mouseY;
extern volatile int byteIndex; // packet order
extern volatile int packetSize;
extern volatile bool newData;
extern volatile int * interval_timer_ptr;
extern const int jazzplaza[];
extern volatile int dir;
extern volatile int seq;
extern volatile int chaserDelay;

/* function prototypes */
void dispScore(int score);
void clearChaser(void);
void interval_timer_ISR(void *, unsigned int); // May not need this
void PS2_ISR(void *, unsigned int);
void audio_ISR(void *, unsigned int);
void drawSplash(alt_up_video_dma_dev * pixel_dma_dev, int buffer, const short splashScreen[]);
void binRandS(short a[], short b[]);
void fadeBGM(alt_up_audio_dev * audio_dev);
void dissolveEffect(alt_up_video_dma_dev * pixel_dma_dev, short a[], short b[]);
void charDissolveEffect(alt_up_video_dma_dev * char_dma_dev, short a[], short b[]);
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
void initSplash(alt_up_video_dma_dev * pixel_dma_dev, int buffer, const short splashScreen[]);
void initMouse(alt_up_video_dma_dev * pixel_dma_dev, int colour, struct Point *mouseLoc, struct Point *mousePrevLoc, struct Point *mousePrev2Loc, int buffer);
bool clickedInArea(struct Point mouseClick, struct Point topLeft, struct Point bottomRight);
void displayInstructionsScreen(alt_up_video_dma_dev * pixel_dma_dev, alt_up_video_dma_dev * char_dma_dev);
void charRandS(short a[], short b[]);

// -- Temp --

typedef struct circle_object {
  // variables
  float x;
  float y;
  float x_prev;
  float y_prev;
  float prev_x_acc;
  float x_acc;
  float y_acc;
  float y_end;
  float x_end;
  bool stopped;
  int radius;
  bool stopping;
  int colour;

} circle_object;

typedef struct level_init {
	float ball_1_x;
	float ball_1_x_prev;
	float ball_1_y;
	float ball_1_y_prev;
} Level_Init;

// iterate through all objects and resolve any collisions between them
void resolve_collisions_dynamic();
/*
 * Call all the necessary functions to apply forces on objects, update
 * posisions, and resolve collisions
 */
void update_all(const float dt, alt_up_video_dma_dev * pixel_dma_dev);

void update_gravity();

void check_bounds(int x_lim_min, int x_lim_max, int y_lim_min, int y_lim_max);

void check_stopped(int index);

bool check_game_status(int x_lim1,int x_lim2, int y_lim1, int y_lim2);

void accelerate_dynamic(float acc_x, float acc_y, circle_object* circle);

void capture_ball();

void reset_circles(circle_object* circle, Level_Init* level);

void randomize_position(circle_object* circle);


/* For circle objects (game object) */
const float c_of_rest = 0.8f;
int num_objects = 0;
float game_time = 0;
float time_step = 0;
struct circle_object circles[MAX_CIRCLES];
bool game_status = false;

// -- Temp --

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

	/* Audio sample variables */
	*audioArrayPosition = 0;

    /* Volatile pointer for interval timer */
    interval_timer_ptr = (int *)INTERVAL_TIMER_BASE; // internal timer base address
     *(interval_timer_ptr) = 0;

    /* Arrays for dissolve effect */
    short randXS[240*320];
	short randYS[240*320];
    short randXC[60*80];
	short randYC[60*80];

	/* Initial background state variables */
	bool blackBackground = false;
	bool splashBackground = true;
	//bool exitBackground = false;
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
	struct Point autoModeTopLeft = {294,18};
	struct Point autoModeBottomRight = {319,36};

    // Allocate mouse positions on the stack
    struct Point currMouse = {0};
    struct Point prevMouse = {0};
    struct Point prev2Mouse = {0};



    // Misc Hex
    int score = 0;
    int hTimer = 0;
    int hTimerMax = 100;
    seq = 0;
    chaserDelay = 1;
    dir = 1;

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

    // Clear the segmented LEDs
    clearChaser();

    /* set the interval timer period - for LED chaser */
    int counter = 4000000; // 1/(100 MHz) x (20000000) = 200 msec
    *(interval_timer_ptr + 0x2) = (counter & 0xFFFF);
    *(interval_timer_ptr + 0x3) = (counter >> 16) & 0xFFFF;

    // open the PS2 port; need double pointer to modify the pointer
    PS2_dev_ptr = &PS2_dev;
    if (openPS2Device(PS2_dev_ptr) != 0) {
    	return -1;
    }

    // We haven't started the timer, but still need to register its ISR
    // Register the PS2 mouse ISR as well
    alt_irq_register(0, (void *)&up_dev, (void *)interval_timer_ISR);
    alt_irq_register(6, (void *)&up_dev, (void *)audio_ISR);
    alt_irq_register(7, (void *)&up_dev, (void *)PS2_ISR);

    /* start interval timer, enable its interrupts to drive LED via a timer interrupt */
    *(interval_timer_ptr + 1) = 0x7; // STOP = 0, START = 1, CONT = 1, ITO = 1

    // open the audio port; need double pointer to modify the pointer
    audio_dev_ptr = &audio_dev;
    if (openAudioDevice(audio_dev_ptr) != 0) {
    	return -1;
    }

    alt_up_audio_enable_write_interrupt(up_dev.audio_dev);


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
    initSplash(pixel_dma_dev, BACK_BUFFER, SphearSplash);

    // Randomize array - could dissolve into the main display
    // This takes some time - good to have it while the splash screen displays
    binRandS(randXS, randYS);
    charRandS(randXC, randYC);

    // Resample colours down to 16 bits
    resampleColours(pixel_dma_dev, &colour_black, &colour_white, &colour_blue, &colour_red, &colour_green);

    // Initially draw the mouse in the middle of the display - the cursor is a square
    // void initMouse(pixel_dma_dev, colour_red, currMouse, BACK_BUFFER);
    initMouse(pixel_dma_dev, colour_red, &currMouse, &prevMouse, &prev2Mouse, BACK_BUFFER);

    // For fun and testing, draw a circle there as well
    //alt_up_video_dma_draw_circle(pixel_dma_dev, color_green_x, currMouse.x-3, currMouse.y-3, 9, circle9, BACK_BUFFER);

    // Swap front to back buffers
    //alt_up_video_dma_ctrl_swap_buffers(pixel_dma_dev);

    // Display initial score on HEX displays
    dispScore(score);

    // for testing circle stuff - should remove
//    int cy = 0;
//    int cx = 0;
//    int cdir = 1;
//    bool releaseCircle=false;
//    bool grabCircle=false;
    int levelSelected = 0;

    Level_Init level_1 = {60, 60, 50, 50};
    bool user_circle_drawn = false;
    // For the physics
    const float dt = 0.2;
//    circles[0].radius = 9;
//	circles[0].x = 15;
//	circles[0].y = 10;
//	circles[0].x_acc = 0;
//	circles[0].y_acc = GRAVITY_CONST;
//	circles[0].x_prev = 15;
//	circles[0].y_prev = 10;
////	circles[0].prev_x_acc = 0;
//	num_objects++;
	circles[1].radius = 9;
	circles[1].x = level_1.ball_1_x;
	circles[1].y = level_1.ball_1_y;
	circles[1].x_acc = 0;
	circles[1].y_acc = GRAVITY_CONST;
	circles[1].x_prev = level_1.ball_1_x_prev;
	circles[1].y_prev = level_1.ball_1_y_prev;
	circles[1].prev_x_acc = 0;
	circles[1].colour = colour_green;
	num_objects++;

	bool all_stopped = false;
	int time_stopped = 0;
	bool debounce_check = false;
	bool debounce_score = false;
	//int h_score_1 = 0xFF;
	//int h_score_2 = 0xFF;

	bool auto_mode = false;
	//bool auto_usr_circle = false;
	bool auto_reset_circle = false;
    // For testing float speed
//    const int numIterations = 500;
//    float a;
//    float b;

    while (1) {
//        a = 3.14f;
//        b = 2.71f;
//        for (int i = 0; i < numIterations; ++i) {
//            a += b;
//        }
//
//        a = 3.14f;
//        b = 2.71f;
//        for (int i = 0; i < numIterations; ++i) {
//            a *= b;
//        }

        // Uncomment this to sync with the timer as opposed to screen refresh
        // while (!timeout)
        //     ; // wait to synchronize with timeout, which is set by the timer ISR
        /*
    	hTimer++;
    	if (hTimer > hTimerMax) {
    		dispScore(score++);
        	if (score >= 99) {
        	score = 0;
        	};
    		hTimer = 0;
    	}
		*/
    	// Check if screen swap completed
    	if (alt_up_video_dma_ctrl_check_swap_status(pixel_dma_dev) == 0) {

    		// Clear prev mouse move - black or splash/exit screen
    		if (blackBackground || displayInstructions) {
    			if (blackBackground) {
    				alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);
    				// Draw exit box and text
    				alt_up_video_dma_draw_box(pixel_dma_dev, colour_red, 294, 0, 319, 18, BACK_BUFFER, 1);

    				// Draw the end zone
    				alt_up_video_dma_draw_box(pixel_dma_dev, colour_red, box_x1, box_y1, box_x2, box_y2, BACK_BUFFER, 0);

    				// Draw the ball spawn zone
    				alt_up_video_dma_draw_box(pixel_dma_dev, colour_green, legal_x1, legal_y1, legal_x2, legal_y2, BACK_BUFFER, 0);
    				//alt_up_video_dma_draw_string(char_dma_dev, "Exit", 75, 2, BACK_BUFFER);

    				if (levelSelected == 2) {
    					// draw the auto toggle box
    					alt_up_video_dma_draw_box(pixel_dma_dev, colour_green, 294, 18, 319, 36, BACK_BUFFER, 1);
    				}

    			} else {
    				alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);
    			}
    		} else if (splashBackground) {
    			restoreSplash(pixel_dma_dev, prev2Mouse, SphearSplash, BACK_BUFFER);
    		}

    		// Should make hex display the score
    		//HEX_PS2(byte1, byte2, byte3); /* Uncomment for PS2 mouse debugging */

    		// testing circle stuff - should remove
//            if (releaseCircle) {
//				cy += cdir;
//				if (cy==0 || cy==pixel_dma_dev->y_resolution-10) {
//					cdir *= -1;
//				}
//				alt_up_video_dma_draw_circle(pixel_dma_dev, colour_green, cx, cy, 9, circle9, BACK_BUFFER);
//            }

            if (newData) {
            	newData = false;
            	moveMouse(PS2_dev, pixel_dma_dev, &currMouse, &prevMouse, &prev2Mouse);
            }

            // Mouse button actions
            // Should add a routine to check where the mouse clicked
			if (button1Down) {
				//printf("x=%d, y=%d",currMouse.x, currMouse.y);
				//grabCircle = true;
				if (displayInstructions) {
					displayInstructions = false;
					blackBackground = false;
					splashBackground = true;
				    // Display the initial splash screen
					dispScore(score);
					charDissolveEffect(char_dma_dev, randXC, randYC);
					//alt_up_video_dma_screen_clear(char_dma_dev, BACK_BUFFER);
				    initSplash(pixel_dma_dev, BACK_BUFFER, SphearSplash);
				    drawCursor(pixel_dma_dev, colour_green, currMouse, BACK_BUFFER);
				} else {
					drawCursor(pixel_dma_dev, colour_green, currMouse, BACK_BUFFER);
					if ((splashBackground) && (levelSelected == 0) && (clickedInArea(currMouse, level1TopLeft, level1BottomRight) ||
						clickedInArea(currMouse, level2TopLeft, level2BottomRight))) {
						if (clickedInArea(currMouse, level1TopLeft, level1BottomRight)) {
							levelSelected = 1;
						} else {
							levelSelected = 2;
							randomize_position(&circles[1]);
						}
						// Selected level 1 or 2 dissolve to main play screen
						blackBackground = true;
						splashBackground = false;
						dispScore(score);
						dissolveEffect(pixel_dma_dev, randXS, randYS);
						alt_up_video_dma_draw_box(pixel_dma_dev, colour_red, 294, 0, 319, 18, FRONT_BUFFER, 1);
						alt_up_video_dma_draw_string(char_dma_dev, "Exit", 75, 2, FRONT_BUFFER);
						if (levelSelected == 2) {
							alt_up_video_dma_draw_box(pixel_dma_dev, colour_green, 294, 18, 319, 36, FRONT_BUFFER, 1);
							alt_up_video_dma_draw_string(char_dma_dev, "Auto", 75, 6, FRONT_BUFFER);
						}
					} else if (clickedInArea(currMouse, instTopLeft, instBottomRight) && (levelSelected == 0)) {
						displayInstructions = true;
						splashBackground = false;
						dispScore(score);
						dissolveEffect(pixel_dma_dev, randXS, randYS);
						displayInstructionsScreen(pixel_dma_dev, char_dma_dev);
					} else if (clickedInArea(currMouse, exitTopLeft, exitBottomRight) && (levelSelected == 0)) {
						dispScore(score);
						initSplash(pixel_dma_dev, BACK_BUFFER, SphearEnd);
					    /* Stop interval timer, enable its interrupts to drive sound via a timer interrupt */
					    //*(interval_timer_ptr + 1) = 0x8; // STOP = 0, START = 1, CONT = 1, ITO = 1
					    alt_up_ps2_disable_read_interrupt(*PS2_dev_ptr); // disable interrupts from PS/2 port
						alt_up_audio_disable_write_interrupt(up_dev.audio_dev);
					    alt_irq_disable(7);
					    alt_irq_disable(6);
						fadeBGM(up_dev.audio_dev);
					    clearChaser();
					    alt_irq_disable(0);
					    alt_irq_disable_all();
						while (1)
							;
					} else if (clickedInArea(currMouse, levelExitTopLeft, levelExitBottomRight) && (levelSelected != 0)) {
						levelSelected = 0;
						blackBackground = false;
						splashBackground = true;
						dispScore(score);
						dissolveEffect(pixel_dma_dev, randXS, randYS);
						// Display the initial splash screen
						alt_up_video_dma_screen_clear(char_dma_dev, BACK_BUFFER);
					    initSplash(pixel_dma_dev, BACK_BUFFER, SphearSplash);
					    drawCursor(pixel_dma_dev, colour_green, currMouse, BACK_BUFFER);
						reset_circles(&circles[1], &level_1);
						score = 0;
						user_circle_drawn = false;
						all_stopped = false;
						time_stopped = 0;
						num_objects = 1;
						auto_mode = false;
						auto_reset_circle = false;

					} else if (clickedInArea(currMouse, autoModeTopLeft, autoModeBottomRight) && levelSelected == 2) {
						//debounce_check++;
						if (!debounce_check) {
							auto_mode = !auto_mode;
							user_circle_drawn = false;
							all_stopped = false;
							time_stopped = 0;
							num_objects = 1;
							debounce_check = true;
						}


					} else if (blackBackground && !user_circle_drawn) {
						if (!auto_mode) {
							debounce_score = false;
							const int m_x = currMouse.x;
							const int m_y = currMouse.y;

							if (m_x > legal_x1 && m_x < legal_x2 && m_y > legal_y1 && m_y < legal_y2) {
								circles[0].x = m_x;
								circles[0].y = m_y;
								circles[0].x_prev = m_x;
								circles[0].y_prev = m_y;
								circles[0].x_acc = 0;
								circles[0].y_acc = GRAVITY_CONST;
								circles[0].radius = 9;
								circles[0].prev_x_acc = 0;
								circles[0].colour = colour_blue;
								circles[0].stopped = false;
								num_objects++;
								user_circle_drawn = true;
							}

						}

					}
				}
			} else if (button2Down) {
				user_circle_drawn = false;
				if (levelSelected == 1) {
					reset_circles(&circles[1], &level_1);
					all_stopped = false;
					time_stopped = 0;
				}
				else if (levelSelected == 2) {
					randomize_position(&circles[1]);
					all_stopped = false;
					time_stopped = 0;
				}
				alt_up_video_dma_draw_string(char_dma_dev, "       ", 10, 10, BACK_BUFFER);
				num_objects = 1;
				if (!debounce_score) {
					score++;
					debounce_score = true;
				}
				dispScore(score);
				drawCursor(pixel_dma_dev, colour_white, currMouse, BACK_BUFFER);
			} else if (auto_mode && !auto_reset_circle && blackBackground && !user_circle_drawn) {
				alt_up_video_dma_draw_string(char_dma_dev, "       ", 10, 10, BACK_BUFFER);
				const int p_x = rand() % legal_x2;
				const int p_y = (rand() % 21) + 10;
				const int a_x = (rand() % 3) - 1;
				circles[0].x = p_x;
				circles[0].y = p_y;
				circles[0].x_prev = p_x + a_x;
				circles[0].y_prev = p_y;
				circles[0].x_acc = 0;
				circles[0].y_acc = GRAVITY_CONST;
				circles[0].radius = 9;
				circles[0].prev_x_acc = 0;
				circles[0].colour = colour_blue;
				circles[0].stopped = false;
				num_objects++;
				user_circle_drawn = true;
				score = 0;

    		} else if (auto_reset_circle && auto_mode) {
    			user_circle_drawn = false;
    			randomize_position(&circles[1]);
				all_stopped = false;
				time_stopped = 0;
				auto_reset_circle = false;
				num_objects = 1;
    		} else {
//				if (!grabCircle) {
//					//alt_up_video_dma_draw_circle(pixel_dma_dev, color_green_x, (pixel_dma_dev->x_resolution / 2)-3, (pixel_dma_dev->y_resolution / 2)-3, 9, circle9, BACK_BUFFER);
//				}
    			debounce_check = false;
				drawCursor(pixel_dma_dev, colour_red, currMouse, BACK_BUFFER);
			}

			if (blackBackground) {
				if (levelSelected == 1) {
					alt_up_video_dma_draw_string(char_dma_dev, "Level 1 Selected", 1, 1, BACK_BUFFER);
				}
				else
					alt_up_video_dma_draw_string(char_dma_dev, "Level 2 Selected", 1, 1, BACK_BUFFER);

				if (user_circle_drawn) {
					update_all(dt, pixel_dma_dev);
					all_stopped = true;
					if (auto_mode) {
						for (int i = 0; i < num_objects; i++) {
							if (circles[i].stopped == false) {
								all_stopped = false;
							}
						}
					} else {
						for (int i = 1; i < num_objects; i++) {
							if (circles[i].stopped == false) {
								all_stopped = false;
							}
						}
					}

					if (all_stopped) {

						if (time_stopped >= 5) {
							if (auto_mode) {
								auto_reset_circle = true;
							}
							else if (check_game_status(box_x1, box_x2, box_y1, box_y2) && !auto_mode) {
								alt_up_video_dma_draw_string(char_dma_dev, "You Win", 10, 10, BACK_BUFFER);
								score = 0;
							}

						} else {
							time_stopped++;
						}
					} else {
						time_stopped = 0;
					}
				}
			}
            /* Swap buffers and wait at the top */
            alt_up_video_dma_ctrl_swap_buffers(pixel_dma_dev);
        }
    }

}

void clearChaser(void) {
    volatile int * HEX3_HEX0_ptr = (int *)HEX3_HEX0_BASE;
    volatile int * HEX5_HEX4_ptr = (int *)HEX5_HEX4_BASE;
    unsigned char hex_segs[] = {0, 0, 0, 0, 0, 0, 0, 0};

	*(HEX3_HEX0_ptr) = ((*(HEX3_HEX0_ptr)& 0x7FFF) | *(int *)(hex_segs));
	*(HEX5_HEX4_ptr) = *(int *)(hex_segs + 4);
}

void dispScore(int score) {
    volatile int * HEX3_HEX0_ptr = (int *)HEX3_HEX0_BASE;

    /* SEVEN_SEGMENT_DECODE_TABLE gives the on/off settings for all segments in
     * a single 7-seg display, for the hex digits 0 - F */
    unsigned char seven_seg_decode_table[] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
        0x7F, 0x67, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};
    unsigned char hex_segs[] = {0, 0, 0, 0, 0, 0, 0, 0};

    if (score < 10) {
    	hex_segs[0]  = seven_seg_decode_table[score];
    	hex_segs[1]  = seven_seg_decode_table[0];
    } else {
    	hex_segs[0]  = seven_seg_decode_table[score % 10];
    	hex_segs[1]  = seven_seg_decode_table[score / 10];
    }

    /* drive the hex displays */
	*(HEX3_HEX0_ptr) = ((*(HEX3_HEX0_ptr)& 0xFFFF8000) | *(int *)(hex_segs));
}

void dissolveEffect(alt_up_video_dma_dev * pixel_dma_dev, short a[], short b[]) {

	alt_up_video_dma_screen_clear(pixel_dma_dev, BACK_BUFFER);
	for (int i=0; i < 240*320; i++) {
		alt_up_video_dma_draw(pixel_dma_dev, 0, a[i], b[i], FRONT_BUFFER);
	}
}

void charDissolveEffect(alt_up_video_dma_dev * char_dma_dev, short a[], short b[]) {

	for (int i=0; i < 60*80; i++) {
		alt_up_video_dma_draw_string(char_dma_dev, " ", a[i], b[i], BACK_BUFFER);
		for (int j=0; j < 200; j++) {
			// Char erase is very fast
		}
	}
}

void drawSplash(alt_up_video_dma_dev * pixel_dma_dev, int buffer, const short splashScreen[]) {
    int index = 0;
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 320; ++x) {
            alt_up_video_dma_draw(pixel_dma_dev, splashScreen[index], x, y, buffer);
            index++;
        }
    }
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

void charRandS(short a[], short b[]) {
	int x, y, i, val, tmp;

	for (y=0; y<60;y++)
		for (x=0; x<80;x++) {
			a[y*80+x]=x;
			b[y*80+x]=y;
		}

	for (i=0;i<80*60;i++) {
		val = (rand() % (80*60 - i)) + i;
		tmp = a[i];
		a[i] = a[val];
		a[val] = tmp;
		tmp = b[i];
		b[i] = b[val];
		b[val] = tmp;
	}
}

void fadeBGM(alt_up_audio_dev * audio_dev) {
	// Loop and fade music
	// Fill the audio FIFO buffer is there's room
	// It's a mono sound; left and right FIFO buffers should be the same
	// fifospace = alt_up_audio_read_fifo_avail (audio_dev, ALT_UP_AUDIO_RIGHT);
	// Is there room?
	float fadeRate = 1.0f / (audioArraySize);
	float fade = 1.0f;
	int buf = 0;
	for (int i=0; i <= audioArraySize/27; i++) {
		chaserDelay = i/1500;
		volatile int * audio_ptr = (int *)AUDIO_SUBSYSTEM_AUDIO_BASE;
		int fifospace = *(audio_ptr + 1); // read the audio port fifospace register
		if ((fifospace & 0x00FF0000) > BUF_THRESHOLD) {
			// Some room; keep feeding the FIFO buffers until they are full
			while ((fifospace & 0x00FF0000) && (*audioArrayPosition <= audioArraySize)) {
				// write to audio buffers from saved data array
				buf = jazzplaza[*audioArrayPosition] * fade;
				alt_up_audio_write_fifo (audio_dev, &buf, 1, ALT_UP_AUDIO_RIGHT);
				alt_up_audio_write_fifo (audio_dev, &buf, 1, ALT_UP_AUDIO_LEFT);
				fade -= fadeRate;
				// Increment audio array position and check if we're at the end of the array
				++(*audioArrayPosition);
				if (*audioArrayPosition >= audioArraySize) {
					// Loop sample
					*audioArrayPosition = 0;
				}
				// Check how much FIFO room is left
				fifospace = *(audio_ptr + 1); // read the audio port fifospace register
			}
		} else {
			for (int j=0; j < 10000; j++) {
				// wait for buffer to empty a bit
			}
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
	else
		currMouse->x = prevMouse->x + (int)mouseX;

	if ((currMouse->y + mouseY < 0) || (currMouse->y + mouseY > (pixel_dma_dev->y_resolution - CURSOR_SIZE - 1)))
		if (currMouse->y + mouseY < 0) {
			currMouse->y = 0;
		} else {
			currMouse->y = pixel_dma_dev->y_resolution - CURSOR_SIZE - 1;
		}
	else
		currMouse->y = prevMouse->y + (int)mouseY;

    copyPoint(prevMouse, prev2Mouse);
    copyPoint(currMouse, prevMouse);

	// Clear the mouse movement queue
	(void)alt_up_ps2_clear_fifo(PS2_dev);

}

int openPS2Device(alt_up_ps2_dev **PS2_dev_ptr) {
	*PS2_dev_ptr = alt_up_ps2_open_dev(PS2_PORT_NAME);
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
    *audio_dev_ptr = alt_up_audio_open_dev (AUDIO_SUBSYSTEM_AUDIO_NAME);
    if ( *audio_dev_ptr == NULL) {
    	alt_printf ("Error: could not open audio device \n");
    	return -1;
    } else {
    	alt_printf ("Opened audio device \n");
    	up_dev.audio_dev = *audio_dev_ptr;
        alt_up_audio_reset_audio_core(up_dev.audio_dev);
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
    	*colour_black	= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(*colour_black);
        *colour_white	= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(*colour_white);
        *colour_blue		= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(*colour_blue);
        *colour_red		= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(*colour_red);
        *colour_green	= ALT_UP_VIDEO_RESAMPLE_RGB_24BIT_TO_16BIT(*colour_green);
    }
}

int openPixelDMA(alt_up_video_dma_dev **pixel_dma_dev_ptr) {
	*pixel_dma_dev_ptr = alt_up_video_dma_open_dev(VGA_SUBSYSTEM_VGA_PIXEL_DMA_NAME);
	if (*pixel_dma_dev_ptr == NULL) {
		alt_printf("Error: could not open VGA's DMA controller device\n");
		return -1;
	} else {
		alt_printf("Opened VGA's DMA controller device\n");
		return 0;
	}
}

int openCharDMA(alt_up_video_dma_dev **char_dma_dev_ptr) {
	*char_dma_dev_ptr = alt_up_video_dma_open_dev(VGA_SUBSYSTEM_CHAR_BUF_SUBSYSTEM_CHAR_BUF_DMA_NAME);
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

void initSplash(alt_up_video_dma_dev * pixel_dma_dev, int buffer, const short splashScreen[]) {

	drawSplash(pixel_dma_dev, buffer, splashScreen);

	// Swap to show the splash screen
	alt_up_video_dma_ctrl_swap_buffers(pixel_dma_dev);
		while (alt_up_video_dma_ctrl_check_swap_status(pixel_dma_dev))
		  ;

	drawSplash(pixel_dma_dev, buffer, splashScreen);
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

	int x, y = 0;

//	while (y < 60) {
//		while (x < 62) {
//			alt_up_video_dma_draw_string(char_dma_dev, "U of T Engineering", x, y, BACK_BUFFER);
//			x += 19;
//		}
//		x = 0;
//		y++;
//	}
	alt_up_video_dma_draw_string(char_dma_dev, "Instructions", 0, 0, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "Click anywhere to return", 0, 3, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "Welcome!", 0, 6, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "The objective of this game is to place your ball in the green ", 0, 9, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "rectangle using the mouse so that it pushes the other ball", 0, 12, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "into the red rectangle", 0, 15, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "The blue ball is user-controlled, and the green ball is game-controlled", 0, 18, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "There are two levels: ", 0, 21, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "In level 1, the target ball's starting position is fixed", 0, 24, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "In level 2, the target ball's starting position is random ", 0, 27, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "Additionally, there is an auto mode, where the game will play itself", 0, 30, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "Score is based on the number of attempts you took to complete the level", 0, 33, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "which is displayed on the Seven Segment LED's", 0, 36, BACK_BUFFER);
	alt_up_video_dma_draw_string(char_dma_dev, "Good Luck!", 0, 39, BACK_BUFFER);
}

// Temp --


// function to resolve collisions between dynamic objects, again thanks to
// johnBuffer on GitHub
void resolve_collisions_dynamic() {
  for (short i = 0; i < num_objects; ++i) {
    circle_object* obj1 = &circles[i];

    // get the second object to check against
    for (short j = i + 1; j < num_objects; ++j) {
      circle_object* obj2 = &circles[j];

      // components of the distance vector
      const float diff_x = obj1->x - obj2->x;
      const float diff_y = obj1->y - obj2->y;
      // distance between objects, accounting for
      const float distance_diag = (diff_x * diff_x) + (diff_y * diff_y);
      // the minimum separation needed to avoid a collision
      const int req_sep = obj1->radius + obj2->radius;

      // Note: comparing distances squared is much quicker than the square
      // root'ed comparison

      // check if the objects are overlapping
      if (distance_diag <= req_sep * req_sep) {
        // distances between objects after we determined they are overlapping
        const float act_distance = sqrtf(distance_diag);
        const float new_x = diff_x / act_distance;
        const float new_y = diff_y / act_distance;

        // change in position
        float move_dist = act_distance - req_sep;

        // now we can update the positions
        obj1->x -= new_x * 0.5F * move_dist;
        obj1->y -= new_y * 0.5f * move_dist;
        obj2->x += new_x * 0.5f * move_dist;
        obj2->y += new_y * 0.5f * move_dist;

        if (new_x > 0) {
		  // object 1 goes right
		  accelerate_dynamic(9, 0, obj1);
		  accelerate_dynamic(-9, 0, obj2);
		} else {
		  accelerate_dynamic(-9, 0, obj1);
		  accelerate_dynamic(9, 0, obj2);
		}

      }
    }
  }
}

void update_gravity() {
  // update gravity on all objects
  for (short i = 0; i < num_objects; ++i) {
    circle_object* temp_obj = &circles[i];
    temp_obj->x_acc = 0;
    temp_obj->y_acc = GRAVITY_CONST;
  }
}

void check_bounds(int x_lim_min, int x_lim_max, int y_lim_min, int y_lim_max) {
  for (short i = 0; i < num_objects; ++i) {
    circle_object* temp_obj = &circles[i];

    // need to set the resultant movement vector after it touches a
    // wall/floor/celing of the screen get the previous position of the object,
    // flip it's direction, and multiply velocity by the coefficient of
    // restitution
    if (temp_obj->x > x_lim_max - temp_obj->radius) {
      // hit right wall
      temp_obj->x_prev = temp_obj->x;
      temp_obj->x = x_lim_max - temp_obj->radius;
      temp_obj->x_acc = 0;
      accelerate_dynamic(9, 0, temp_obj);
      temp_obj->prev_x_acc = temp_obj->x_acc;
      temp_obj->stopping = true;

    } else if (temp_obj->x < x_lim_min + temp_obj->radius) {
      // hit left wall
      temp_obj->x_prev = temp_obj->x;
      temp_obj->x = x_lim_min + temp_obj->radius;
      temp_obj->x_acc = 0;
      accelerate_dynamic(-9, 0, temp_obj);
      temp_obj->prev_x_acc = temp_obj->x_acc;
      temp_obj->stopping = true;
    }
    if (temp_obj->y > y_lim_max - temp_obj->radius) {
      temp_obj->y = temp_obj->y_prev;
      temp_obj->y_prev = y_lim_max - temp_obj->radius;
      accelerate_dynamic(0, -2, temp_obj);

    } else if (temp_obj->y < y_lim_min + temp_obj->radius) {
      temp_obj->y = temp_obj->y_prev;
      temp_obj->y_prev = y_lim_min + temp_obj->radius;
      accelerate_dynamic(0, -2, temp_obj);
    }
  }
}


// update the position of this circle
void update_dynamic(const float dt, circle_object* circle) {
  const float x_vel = circle->x - circle->x_prev;
  const float y_vel = circle->y - circle->y_prev;

  circle->x_prev = circle->x;
  circle->y_prev = circle->y;

  // Verlet Integration
  circle->x = circle->x + x_vel + circle->x_acc * (dt * dt);
  circle->y = circle->y + y_vel + circle->y_acc * (dt * dt);

  if((circle->x - circle->x_prev)*(x_vel)<0 && circle->stopping){
	circle->x = circle ->x_prev;
	circle->x_acc = 0;
	circle->prev_x_acc = 0;
  }

}

// set the new acceleration of the object
void accelerate_dynamic(float acc_x, float acc_y, circle_object* circle) {
  circle->x_acc += acc_x;
  circle->y_acc += acc_y;
}

void set_new_velocity_dynamic(float x_pos, float y_pos, float dt,
                              circle_object* circle) {
  circle->x_prev = circle->x - (x_pos * dt);
  circle->y_prev = circle->y - (y_pos * dt);
}

void add_velocity_dynamic(float x_pos, float y_pos, float dt,
                          circle_object* circle) {
  circle->x_prev -= x_pos * dt;
  circle->y_prev -= y_pos * dt;
}

float get_vel_x(float dt, circle_object* circle) {
  return (circle->x - circle->x_prev) / dt;
}

float get_vel_y(float dt, circle_object* circle) {
  return (circle->y - circle->y_prev) / dt;
}

void update_all(const float dt, alt_up_video_dma_dev * pixel_dma_dev) {
  update_gravity();
  resolve_collisions_dynamic();
  //check_bounds(270,319,0,239);
  check_bounds(0,320,0,240);
  capture_ball();
  for (int i = 0; i < num_objects; i++) {
    circle_object* temp_circ = &circles[i];
    if (temp_circ->prev_x_acc != 0) {
      accelerate_dynamic(temp_circ->prev_x_acc, 0, temp_circ);
    }
    update_dynamic(dt, temp_circ);
    alt_up_video_dma_draw_circle(pixel_dma_dev, temp_circ->colour, temp_circ->x, temp_circ->y, 9, circle9, BACK_BUFFER);
    //draw_circle(temp_circ->x, temp_circ->y, 9, circle9, rgb(255, 174, 66));
    check_stopped(i);
  }
   //draw_line(319,239,270,239, rgb(255, 174, 66));
   //draw_line(270,239,270,200, rgb(255, 174, 66));
   //draw_line (319,200,319,239, rgb(255, 174, 66));
}

// return true if the object has stopped and modify x_end and y_end to the ending position of the circle
void check_stopped(int index) {
  circle_object* temp = &circles[index];
  if ((int)(fabs(temp->x - temp->x_prev)) == 0 && (int)(fabs(temp->y - temp->y_prev)) == 0) {
    temp->x_end = temp->x;
    temp->y_end = temp->y;
    temp->stopped = true;
    //*ledPtr = 0xff;
  } else {
    temp->stopped = false;
    // *ledPtr = 1;
  }
}


// return true if all the balls are within the bounds
// should only be called after all the balls are stopped
bool check_game_status(int x_min, int x_max, int y_min, int y_max) {
  bool status = true;
  for (int i = 1; i < num_objects; i++) {
    circle_object temp = circles[i];
    bool temp_status;
    if (temp.x_end > x_min && temp.x_end < x_max && temp.y_end > y_min &&
        temp.y_end < y_max) {
      temp_status = true;
    } else {
      temp_status = false;
    }
    status = status & temp_status;
  }
  return status;
}

void capture_ball() {
  // when the ball enters the region of the end zone, disable changes in x direction and only apply gravity
  for (int i = 1; i < num_objects; i++) {
    circle_object* temp = &circles[i];
    if (temp->x > box_x1 && temp->x < box_x2 && temp->y > box_y1 &&
        temp->y < box_y2) {
      temp->x_prev = temp->x;
      temp->x_acc = 0;
      temp->prev_x_acc = 0;
    }
  }
}

void reset_circles(circle_object* circle, Level_Init* level) {
	circle->x = level->ball_1_x;
	circle->x_prev = level->ball_1_x_prev;
	circle->y = level->ball_1_y;
	circle->y_prev = level->ball_1_y_prev;
	circle->stopped = false;
	circle->stopping = false;
	circle->prev_x_acc = 0;
	circle->x_acc = 0;
	circle->y_acc = GRAVITY_CONST;
}

void randomize_position(circle_object* circle) {
	const int x_pos = (rand() % (legal_x2));
	circle->x = x_pos;
	circle->x_prev = x_pos;
	const int y_pos = (rand() % (Y_RAND_MAX - circle->radius - 40) + 40);
	circle->y = y_pos;
	circle->y_prev = y_pos;
}
