#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define NUM_SQUARES 8
#define BOX_SIZE 5
#define VGA_SUBSYSTEM_VGA_PIXEL_DMA_BASE 0xff203020
#define MAX_CIRCLES 2
#define GRAVITY_CONST 10

// iterate through all objects and resolve any collisions between them
void resolve_collisions_dynamic();
/*
 * Call all the necessary functions to apply forces on objects, update
 * posisions, and resolve collisions
 */
void update_all();

void update_gravity();

void check_bounds(int x_lim_min, int x_lim_max, int y_lim_min, int y_lim_max);

void check_stopped(int index);

bool check_game_status(int x_lim1,int x_lim2, int y_lim1, int y_lim2);

volatile int pixel_buffer_start;                     // global variable
volatile int* status_reg_global = (int*)0xFF20302C;  // global
volatile int* ctrl_reg = (int*)0xFF203020;
volatile int* ledPtr = (int*)0xFF200000;

int circle9[9][9] = {{0, 0, 1, 1, 1, 1, 1, 0, 0}, {0, 1, 1, 1, 1, 1, 1, 1, 0},
                     {1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1},
                     {1, 1, 1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1, 1, 1},
                     {1, 1, 1, 1, 1, 1, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 1, 1, 0},
                     {0, 0, 1, 1, 1, 1, 1, 0, 0}};

int res_offset;
int col_offset;
int screen_x;
int screen_y;
bool game_status;

// Thanks to johnBuffer on GitHub for the inspiration in this struct and
// collision resolution
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
  bool stop;
  // bool added_acc;// always init to false

} circle_object;

void accelerate_dynamic(float acc_x, float acc_y, circle_object* circle);

/* For circle objects (game object) */
const float c_of_rest = 0.8f;
int num_objects = 0;
float game_time = 0;
float time_step = 0;
struct circle_object circles[MAX_CIRCLES];

short Buffer1[240][512];  // 240 rows, 512 (320 + padding) columns
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
  volatile short int* one_pixel_address;

  one_pixel_address = (short*)(pixel_buffer_start + (y << 10) + (x << 1));
  *one_pixel_address = line_color;
}

void draw_line(int x0, int y0, int x1, int y1, short colour) {
  bool is_steep = abs(y1 - y0) > abs(x1 - x0);

  if (is_steep) {
    swap(&x0, &y0);
    swap(&x1, &y1);
  }
  if (x0 > x1) {
    swap(&x0, &x1);
    swap(&y0, &y1);
  }

  int delta_x = x1 - x0;
  int delta_y = abs(y1 - y0);
  int err = -(delta_x / 2);
  int y = y0;
  int y_step = 0;

  if (y0 < y1) {
    y_step = 1;
  } else {
    y_step = -1;
  }

  for (int x = x0; x < x1; ++x) {
    if (is_steep) {
      plot_pixel(y, x, colour);
    } else {
      plot_pixel(x, y, colour);
    }

    err = err + delta_y;

    if (err > 0) {
      y = y + y_step;
      err = err - delta_x;
    }
  }
}

void draw_circle(int x1, int y1, int size, int circle[size][size],
                 short pixel_color) {
  int x2 = x1 + size - 1;
  int y2 = y1 + size - 1;
  int x, y = 0;
  int pixel_buf_ptr = *(int*)VGA_SUBSYSTEM_VGA_PIXEL_DMA_BASE;
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
      pixel_ptr =
          pixel_buf_ptr + (row << (10 - res_offset - col_offset)) + (col << 1);
      if (circle[x][y]) *(short*)pixel_ptr = pixel_color;  // set pixel color
      x++;
    }
    x = 0;
    y++;
  }
}

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
        obj1->x += new_x * 0.5F * move_dist;
        obj1->y += new_y * 0.5f * move_dist;
        obj2->x -= new_x * 0.5f * move_dist;
        obj2->y -= new_y * 0.5f * move_dist;

        if(new_x > 0){
          //object 1 goes right
          accelerate_dynamic(-8,0,obj1);
          accelerate_dynamic(8,0,obj2);
        }
        else{
          accelerate_dynamic(8,0,obj1);
          accelerate_dynamic(-8,0,obj2);
        }
       
      }
    }
  }
}

void update_gravity() {
  // update gravity on all objects
  for (short i = 0; i < MAX_CIRCLES; ++i) {
    circle_object* temp_obj = &circles[i];
    temp_obj->x_acc = 0;
    temp_obj->y_acc = GRAVITY_CONST;
  }
}

void check_bounds(int x_lim_min, int x_lim_max, int y_lim_min, int y_lim_max) {
  for (short i = 0; i < MAX_CIRCLES; ++i) {
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
      accelerate_dynamic(8, 0, temp_obj);
      temp_obj->prev_x_acc = temp_obj->x_acc;

    } else if (temp_obj->x < x_lim_min + temp_obj->radius) {
      // hit left wall
      temp_obj->x_prev = temp_obj->x;
      temp_obj->x = x_lim_min+ temp_obj->radius;
      temp_obj->x_acc = 0;
      accelerate_dynamic(-8, 0, temp_obj);
      temp_obj->prev_x_acc = temp_obj->x_acc;
    }
    if (temp_obj->y > y_lim_max - temp_obj->radius) {
      temp_obj->y = temp_obj->y_prev;
      temp_obj->y_prev = y_lim_max - temp_obj->radius - 3;
      accelerate_dynamic(0, -2, temp_obj);

    } else if (temp_obj->y < y_lim_min + temp_obj->radius) {
      temp_obj->y = temp_obj->y_prev;
      temp_obj->y_prev = y_lim_min + temp_obj->radius + 3;
      accelerate_dynamic(0, -2, temp_obj);
    }
  }
}


// update the position of this circle
void update_dynamic(float dt, circle_object* circle) {
  const float x_vel = circle->x - circle->x_prev;
  const float y_vel = circle->y - circle->y_prev;

  circle->x_prev = circle->x;
  circle->y_prev = circle->y;
  // Verlet Integration
  circle->x = circle->x + x_vel + circle->x_acc * (dt * dt);
  circle->y = circle->y + y_vel + circle->y_acc * (dt * dt);
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

int rgb(unsigned char r, unsigned char g, unsigned char b) {
  if (r < 0 || 255 < r || g < 0 || 255 < g || b < 0 || b > 255) return -1;

  unsigned char red = r >> 3;
  unsigned char green = g >> 2;
  unsigned char blue = b >> 3;

  int result = (red << (5 + 6)) | (green << 5) | blue;

  return result;
}

void update_all() {
  update_gravity();
  resolve_collisions_dynamic();
  //check_bounds(200,319,0,239);
  check_bounds(0,320,0,240);
  for (int i = 0; i < MAX_CIRCLES; i++) {
    circle_object* temp_circ = &circles[i];
    if (temp_circ->prev_x_acc != 0) {
      accelerate_dynamic(temp_circ->prev_x_acc, 0, temp_circ);
    }
    update_dynamic(0.2, temp_circ);
    draw_circle(temp_circ->x, temp_circ->y, 9, circle9, rgb(255, 174, 66));
    check_stopped(i);
  }
   draw_line(319,239,270,239, rgb(255, 174, 66));
   draw_line(270,239,270,200, rgb(255, 174, 66));
   draw_line (319,200,319,239, rgb(255, 174, 66));
}

// return true if the object has stopped and modify x_end and y_end to the ending position of the circle
void check_stopped(int index){
      circle_object * temp = &circles[index];
      if(((int)temp->x == (int)temp->x_prev) && (int)temp->y ==(int)(temp->y_prev)){
        temp->x_end = temp->x;
        temp->y_end = temp->y;
        temp->stopped = true;
      }
      else{
        temp->stopped = false;
      }
    }


// return true if all the balls are within the bounds
// should only be called after all the balls are stopped
bool check_game_status(int x_min,int x_max, int y_min, int y_max){
      bool status = true;
      for(int i = 1; i < MAX_CIRCLES; i++){
        circle_object temp = circles[i];
        bool temp_status;
      if(temp.x_end > x_min && temp.x_end < x_max && temp.y_end > y_min && temp.y_end < y_max){
        temp_status = true;
      }
      else{
        temp_status = false;
      }
      status = status & temp_status;
      }
      return status;
}


int main(void) {
  // location of the front buffer
  volatile int* pixel_ctrl_ptr = (int*)0xFF203020;
  volatile int* status_reg = (int*)0xFF20302C;
  bool game_over = false;

  int db = 16;

  volatile int* video_resolution =
      (int*)(VGA_SUBSYSTEM_VGA_PIXEL_DMA_BASE + 0x8);
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
  circles[0].x = 0;
  circles[0].y = 10;
  circles[0].x_acc = 0;
  circles[0].y_acc = GRAVITY_CONST;
  circles[0].x_prev = 0;
  circles[0].y_prev = 10;
  circles[0].prev_x_acc = 0;
  num_objects++;
  circles[1].radius = 9;
  circles[1].x = 100;
  circles[1].y = 10;
  circles[1].x_acc = 0;
  circles[1].y_acc = GRAVITY_CONST;
  circles[1].x_prev = 100;
  circles[1].y_prev = 10;
  circles[1].prev_x_acc = 0;

  num_objects++;
   
  clear_screen();
  *ledPtr = 0;

  const int dt = 0.2;
  // infinite loop
  while (1) {
    clear_screen();
    
    update_all();

    wait_for_vsync();

    int i;
    for(i = 1; i < MAX_CIRCLES; i++){
      if(!circles[i].stopped){
        break;
      }
    }
    //i == MAX_CIRClES iff all circles are stoppped
    if(i == MAX_CIRCLES){
      if(check_game_status(270,319,200,270)){
        *ledPtr = 1;
      }
      else{
        *ledPtr = 0;
      }
    }

    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
  }
  return 0;
}