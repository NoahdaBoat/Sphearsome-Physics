#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define NUM_SQUARES 8
#define BOX_SIZE 5
#define VGA_SUBSYSTEM_VGA_PIXEL_DMA_BASE 0xff203020
#define MAX_CIRCLES 1
#define GRAVITY_CONST 10

// iterate through all objects and resolve any collisions between them
void resolve_collisions_dynamic();
/*
 * Call all the necessary functions to apply forces on objects, update posisions, and resolve collisions
 */
void update_all();

void update_gravity();

void check_bounds(int x_lim, int y_lim);

volatile int pixel_buffer_start; // global variable
volatile int* status_reg_global = (int*)0xFF20302C; // global
volatile int* ctrl_reg = (int*)0xFF203020;

int res_offset;
int col_offset;
int screen_x;
int screen_y;

// Thanks to johnBuffer on GitHub for the inspiration in this struct and collision resolution
typedef struct circle_object {
	// variables
	float x;
	float y;
	float x_prev;
	float y_prev;
	float x_acc;
	float y_acc;
	int radius;

} circle_object;

/* For circle objects (game object) */
const float c_of_rest = 0.8f;
int num_objects = 0;
float game_time = 0;
float time_step = 0;
struct circle_object circles[MAX_CIRCLES];

short Buffer1[240][512]; // 240 rows, 512 (320 + padding) columns 
short Buffer2[240][512];

void swap(int* zero, int* one) {
	int temp = *zero;
	*zero = *one;
	*one = temp;
}

void clear_screen_pixels(int y) {
	short* one_pixel_address = 0;
	short line_color[320] = {0};
	one_pixel_address = (short*)(pixel_buffer_start + (y << 10));
	memcpy(one_pixel_address, line_color, sizeof(line_color));
}

void clear_screen() {
	for (short i = 0; i < 240; ++i) {
		clear_screen_pixels(i);
	}
}

void draw_box(int x, int y, short colour[]) {
    // memcpy is much faster than iterating through an array
    // plot the box a row at a time
    short* one_pixel_address;
    for (unsigned int i = y; i < y + BOX_SIZE; ++i) {
        one_pixel_address = (short*)(pixel_buffer_start + (i << 10) + (x << 1));
        memcpy(one_pixel_address, colour, BOX_SIZE);
    }
}

void wait_for_vsync() {
    // write 1 into buffer
    *ctrl_reg = 1;
    // wait for the screen to finish drawing the last line
    while ((*status_reg_global & 0x00000001) == 1) {
        // wait
    }
}

void plot_pixel(int x, int y, short line_color) {
    volatile short int *one_pixel_address;

    one_pixel_address = (short*)(pixel_buffer_start + (y << 10) + (x << 1));
    *one_pixel_address = line_color;
}

void draw_line(int x0, int y0, int x1, int y1, short colour) {
	bool is_steep = abs(y1-y0) > abs(x1-x0);

	if (is_steep) {
		swap(&x0, &y0);
		swap(&x1, &y1);

	}
	if (x0 > x1) {
		swap(&x0, &x1);
		swap(&y0, &y1);
	}

	int delta_x = x1 - x0;
	int delta_y = abs(y1-y0);
	int err = -(delta_x/2);
	int y = y0;
	int y_step = 0;

	if (y0 < y1) {
		y_step = 1;
	} 
	else {
		y_step = -1;
	}

	for (int x = x0; x < x1; ++x) {
		if (is_steep) {
			plot_pixel(y, x, colour);
		}
		else {
			plot_pixel(x, y, colour);
		}

		err = err + delta_y;

		if (err > 0) {
			y = y + y_step;
			err = err - delta_x;
		}
	}
}

void draw_circle(int x1, int y1, int size, int circle[size][size], short pixel_color) {
    
	int x2 = x1 + size -1;
	int y2 = y1 + size -1;
	int x, y = 0;
	int pixel_buf_ptr = *(int *)VGA_SUBSYSTEM_VGA_PIXEL_DMA_BASE;
    int pixel_ptr, row, col;
    int x_factor = 0x1 << (res_offset + col_offset);
    int y_factor = 0x1 << (res_offset);
    x1 = x1 / x_factor;
    x2 = x2 / x_factor;
    y1 = y1 / y_factor;
    y2 = y2 / y_factor;

    /* assume  the circle coordinates are valid */
    for (row = y1; row <= y2; row++) {
        for (col = x1; col <= x2; ++col) {
            pixel_ptr = pixel_buf_ptr +
                        (row << (10 - res_offset - col_offset)) + (col << 1);
			if (circle[x][y])
            	*(short *)pixel_ptr = pixel_color; // set pixel color
			x++;
        }
		x = 0;
		y++;
	}
}

// function to resolve collisions between dynamic objects, again thanks to johnBuffer on GitHub
void resolve_collisions_dynamic() {
	for (short i = 0; i < num_objects; ++i) {
		circle_object obj1 = circles[i];

		// get the second object to check against
		for (short j = i+1; j < num_objects; ++j) {
			circle_object obj2 = circles[j];

			// components of the distance vector
			const float diff_x = obj1.x - obj2.x;
			const float diff_y = obj1.y - obj2.y;
			// distance between objects, accounting for
			const float distance_diag = diff_x*diff_x + diff_y*diff_y;
			// the minimum separation needed to avoid a collision
			const int req_sep = obj1.radius + obj2.radius;

			// Note: comparing distances squared is much quicker than the square root'ed comparison

			// check if the objects are overlapping
			if (distance_diag < req_sep*req_sep) {
				// distances between objects after we determined they are overlapping
				const float act_distance = sqrtf(distance_diag);
				const float new_x = diff_x/act_distance;
				const float new_y = diff_y/act_distance;

				// mass ratios are used to determine the resulting direction vectors
				const float mr_1 = obj1.radius/(obj1.radius + obj2.radius);
				const float mr_2 = obj2.radius/(obj1.radius + obj2.radius);
				// change in position
				const float chg_pos = 0.5f * c_of_rest * (act_distance - req_sep);

				// now we can update the positions
				obj1.x -= new_x * (mr_2 * chg_pos);
				obj1.y -= new_y * (mr_2 * chg_pos);
				obj2.x += new_x * (mr_1 * chg_pos);
				obj2.y += new_y * (mr_1 * chg_pos);
			}
		}
	}
}

void update_gravity() {
	// update gravity on all objects
	for (short i = 0; i < num_objects; ++i) {
		circle_object temp_obj = circles[i];
		temp_obj.x_acc = 0;
		temp_obj.y_acc = GRAVITY_CONST;
	}
}

void check_bounds(int x_lim, int y_lim) {
	for (short i = 0; i < num_objects; ++i) {
		circle_object temp_obj = circles[i];
        
        // need to set the resultant movement vector after it touches a wall/floor/celing of the screen
        // get the previous position of the object, flip it's direction, and multiply velocity by the coefficient of restitution
		if (temp_obj.x > x_lim - temp_obj.radius) {
			
		}
		else if (temp_obj.x < 0 + temp_obj.radius) {
				
		}
		
		if (temp_obj.y > y_lim - temp_obj.radius) {
			// float temp = temp_obj.y;
            // temp_obj.y = temp_obj.y_prev;
            // temp_obj.y_prev = temp;
            
		}
		else if (temp_obj.y < 0 + temp_obj.radius) {
			
		}
	}
}

// update the position of this circle
void update_dynamic(float dt, circle_object* circle) {
    const float x_vel = circle->x - circle->x_prev;
    const float y_vel = circle->y - circle->x_prev;

    circle->x_prev = circle->x;
    circle->y_prev = circle->y;

    // Verlet Integration
    circle->x = circle->x + x_vel + circle->x_acc * (dt*dt);
    circle->y = circle->y + y_vel + circle->y_acc * (dt*dt);

    circle->x_acc = 0;
    circle->y_acc = 0;
}

// set the new acceleration of the object
void accelerate_dynamic(float acc_x, float acc_y, circle_object* circle) {
    circle->x_acc += acc_x;
    circle->y_acc += acc_y;
}

void set_new_velocity_dynamic(float x_pos, float y_pos, float dt, circle_object* circle) {
    circle->x_prev = circle->x - (x_pos * dt);
    circle->y_prev = circle->y - (y_pos * dt);
}

void add_velocity_dynamic(float x_pos, float y_pos, float dt, circle_object* circle) {
    circle->x_prev -= x_pos * dt;
    circle->y_prev -= y_pos * dt;
}

float get_vel_x(float dt, circle_object* circle) {
    return (circle->x - circle->x_prev)/dt;
}

float get_vel_y(float dt, circle_object* circle) {
    return (circle->y - circle->y_prev)/dt;
}

int rgb(unsigned char r, unsigned char g, unsigned char b) {
    if (r < 0 || 255 < r || g < 0 || 255 < g || b < 0 || b > 255)
        return -1;

    unsigned char red = r >> 3;
    unsigned char green = g >> 2;
    unsigned char blue = b >> 3;

    int result = (red << (5 + 6)) | (green << 5) | blue;

//tests
   // printf("red: %x\n", red);
   // printf("green: %x\n", green);
   // printf("blue: %x\n", blue);
   // printf("result: %x\n", result);

    return result;
}

int main(void) {
    // location of the front buffer
    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
    volatile int* status_reg = (int*)0xFF20302C;

    int circle9[9][9] = {{0,0,1,1,1,1,1,0,0},
                        {0,1,1,1,1,1,1,1,0},
                        {1,1,1,1,1,1,1,1,1},
                        {1,1,1,1,1,1,1,1,1},
                        {1,1,1,1,1,1,1,1,1},
                        {1,1,1,1,1,1,1,1,1},
                        {1,1,1,1,1,1,1,1,1},
                        {0,1,1,1,1,1,1,1,0},
                        {0,0,1,1,1,1,1,0,0}};

    int db = 16;

    volatile int * video_resolution = (int *)(VGA_SUBSYSTEM_VGA_PIXEL_DMA_BASE + 0x8);
    screen_x = *video_resolution & 0xFFFF;
    screen_y = (*video_resolution >> 16) & 0xFFFF;

    *(pixel_ctrl_ptr + 1) = (int)&Buffer1;

        /* check if resolution is smaller than the standard 320 x 240 */
    res_offset = (screen_x == 160) ? 1 : 0;

    /* check if number of data bits is less than the standard 16-bits */
    col_offset = (db == 8) ? 1 : 0;

    wait_for_vsync();

    // location of the status register
    

    /* Read location of the pixel buffer from the pixel buffer controller */
    pixel_buffer_start = *pixel_ctrl_ptr;
    status_reg_global = status_reg;
    clear_screen();

    *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    clear_screen();

    circles[0].radius = 9;
    circles[0].x = 10;
    circles[0].y = 10;
    circles[0].x_acc = 0;
    circles[0].y_acc = GRAVITY_CONST;
    circles[0].x_prev = 10;
    circles[0].y_prev = 10;
    // info on the squares. x and y are of the top left (i.e. 0,0)
    // int squares_x[NUM_SQUARES];
    // int squares_y[NUM_SQUARES];
    // which direction to increment the position of the box by
    // int step_x[NUM_SQUARES];
    // int step_y[NUM_SQUARES];
    // short colour[NUM_SQUARES];
    // int square_size = BOX_SIZE; // BOX_SIZExBOX_SIZE square

    //setup the position of the boxes
    // for (unsigned int i = 0; i < NUM_SQUARES; ++i) {
    //     // account for size of square
    //     squares_x[i] = rand() % (320-BOX_SIZE);
    //     squares_y[i] = rand() % (240-BOX_SIZE);
       
    //     // get the starting step directions, 
    //     // need to separate to ensure it won't move purely diagonally
    //     if (rand() % 2) {
    //         step_x[i] = 1;
    //     }
    //     else {
    //         step_x[i] = -1;
    //     }
    //     // do the same for y
    //     if (rand() % 2) {
    //         step_y[i] = 1;
    //     }
    //     else {
    //         step_y[i] = -1;
    //     }
    //     colour[i] = 0x001F; // blue box
    // }

    clear_screen();

    const int dt = 0.2;
    int j = 5;
    // infinite loop
    while (1) {
        
        clear_screen();
        
        // for (unsigned int i = 0; i < NUM_SQUARES; ++i) {
        //     // connect the start of the line to the next box
        //     // unless it is the last one, then connect it to the first
        //     draw_box(squares_x[i], squares_y[i], colour);
        //     if (i < NUM_SQUARES - 1) {   
        //         draw_line((squares_x[i] + (BOX_SIZE/2) + 1), (squares_y[i] + (BOX_SIZE/2) + 1), (squares_x[i+1] + (BOX_SIZE/2) + 1), (squares_y[i+1] + (BOX_SIZE/2) + 1), colour[i]);
        //     }
        //     else {
        //         draw_line((squares_x[i] + (BOX_SIZE/2) + 1), (squares_y[i] + (BOX_SIZE/2) + 1), (squares_x[0] + (BOX_SIZE/2) + 1), (squares_y[0] + (BOX_SIZE/2) + 1), colour[i]);
        //     }
            
        //     // // increment position
        //     squares_x[i] += step_x[i];
        //     squares_y[i] += step_y[i];

        //     // check if the the step for each box needs to be changed
        //     if (squares_x[i]+BOX_SIZE-1 >= 320 || squares_x[i] <= 0) {
        //         step_x[i] *= -1;
        //     }
        //     if (squares_y[i]+BOX_SIZE-1 >= 240 || squares_y[i] <= 0) {
        //         step_y[i] *= -1;
        //     }
        // }


        update_gravity();
        check_bounds(320, 240);
        update_dynamic(0.2, &circles[0]);


        draw_circle(circles[0].x, circles[0].y, circles[0].radius, circle9, rgb(255,174,66));
        


        wait_for_vsync();

        pixel_buffer_start = *(pixel_ctrl_ptr + 1);
        
    }
    return 0;
}