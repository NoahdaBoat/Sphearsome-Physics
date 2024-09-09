///*
// * physics.c
// *
// *  Created on: Apr 6, 2024
// *
// */
//
//#include <math.h>
//#include <stdbool.h>
//#include "globals.h"
//#include "circles.h"
//
//#define NUM_SQUARES 8
//#define BOX_SIZE 5
//#define VGA_SUBSYSTEM_VGA_PIXEL_DMA_BASE 0xff203020
//#define MAX_CIRCLES 2
//#define GRAVITY_CONST 10
//
//
//
//volatile int pixel_buffer_start;                     // global variable
//volatile int* status_reg_global = (int*)0xFF20302C;  // global
//volatile int* ctrl_reg = (int*)0xFF203020;
//volatile int* ledPtr = (int*)0xFF200000;
//
//int res_offset;
//int col_offset;
//int screen_x;
//int screen_y;
//bool game_status;
//
//// Thanks to johnBuffer on GitHub for the inspiration in this struct and
//// collision resolution
//
//
//short Buffer1[240][512];  // 240 rows, 512 (320 + padding) columns
//short Buffer2[240][512];
//
//void swap(int* zero, int* one) {
//  int temp = *zero;
//  *zero = *one;
//  *one = temp;
//}
//
//// function to resolve collisions between dynamic objects, again thanks to
//// johnBuffer on GitHub
//void resolve_collisions_dynamic() {
//  for (short i = 0; i < num_objects; ++i) {
//    circle_object* obj1 = &circles[i];
//
//    // get the second object to check against
//    for (short j = i + 1; j < num_objects; ++j) {
//      circle_object* obj2 = &circles[j];
//
//      // components of the distance vector
//      const float diff_x = obj1->x - obj2->x;
//      const float diff_y = obj1->y - obj2->y;
//      // distance between objects, accounting for
//      const float distance_diag = (diff_x * diff_x) + (diff_y * diff_y);
//      // the minimum separation needed to avoid a collision
//      const int req_sep = obj1->radius + obj2->radius;
//
//      // Note: comparing distances squared is much quicker than the square
//      // root'ed comparison
//
//      // check if the objects are overlapping
//      if (distance_diag <= req_sep * req_sep) {
//        // distances between objects after we determined they are overlapping
//        const float act_distance = sqrtf(distance_diag);
//        const float new_x = diff_x / act_distance;
//        const float new_y = diff_y / act_distance;
//
//        // change in position
//        float move_dist = act_distance - req_sep;
//
//        // now we can update the positions
//        obj1->x += new_x * 0.5F * move_dist;
//        obj1->y += new_y * 0.5f * move_dist;
//        obj2->x -= new_x * 0.5f * move_dist;
//        obj2->y -= new_y * 0.5f * move_dist;
//
//      }
//    }
//  }
//}
//
//void update_gravity() {
//  // update gravity on all objects
//  for (short i = 0; i < MAX_CIRCLES; ++i) {
//    circle_object* temp_obj = &circles[i];
//    temp_obj->x_acc = 0;
//    temp_obj->y_acc = GRAVITY_CONST;
//  }
//}
//
//void check_bounds(int x_lim_min, int x_lim_max, int y_lim_min, int y_lim_max) {
//  for (short i = 0; i < MAX_CIRCLES; ++i) {
//    circle_object* temp_obj = &circles[i];
//
//    // need to set the resultant movement vector after it touches a
//    // wall/floor/celing of the screen get the previous position of the object,
//    // flip it's direction, and multiply velocity by the coefficient of
//    // restitution
//    if (temp_obj->x > x_lim_max - temp_obj->radius) {
//      // hit right wall
//      temp_obj->x_prev = temp_obj->x;
//      temp_obj->x = x_lim_max - temp_obj->radius;
//      temp_obj->x_acc = 0;
//      if (temp_obj->prev_x_acc != 0) {
//        accelerate_dynamic(temp_obj->prev_x_acc, 0, temp_obj);
//      } else {
//        accelerate_dynamic(8, 0, temp_obj);
//      }
//      temp_obj->prev_x_acc = temp_obj->x_acc;
//
//    } else if (temp_obj->x < x_lim_min + temp_obj->radius) {
//      // hit left wall
//      temp_obj->x_prev = temp_obj->x;
//      temp_obj->x = x_lim_min+ temp_obj->radius;
//      temp_obj->x_acc = 0;
//      if (temp_obj->prev_x_acc != 0) {
//        accelerate_dynamic(temp_obj->prev_x_acc, 0, temp_obj);
//      } else {
//        accelerate_dynamic(-8, 0, temp_obj);
//      }
//      temp_obj->prev_x_acc = temp_obj->x_acc;
//    }
//    if (temp_obj->y > y_lim_max - temp_obj->radius) {
//      temp_obj->y = temp_obj->y_prev;
//      temp_obj->y_prev = y_lim_max - temp_obj->radius - 3;
//      accelerate_dynamic(0, -2, temp_obj);
//
//    } else if (temp_obj->y < y_lim_min + temp_obj->radius) {
//      temp_obj->y = temp_obj->y_prev;
//      temp_obj->y_prev = y_lim_min + temp_obj->radius + 3;
//      accelerate_dynamic(0, -2, temp_obj);
//    }
//  }
//}
//
//// update the position of this circle
//void update_dynamic(float dt, circle_object* circle) {
//  const float x_vel = circle->x - circle->x_prev;
//  const float y_vel = circle->y - circle->y_prev;
//
//  circle->x_prev = circle->x;
//  circle->y_prev = circle->y;
//
//  // Verlet Integration
//  circle->x = circle->x + x_vel + circle->x_acc * (dt * dt);
//  circle->y = circle->y + y_vel + circle->y_acc * (dt * dt);
//
//}
//
//// set the new acceleration of the object
//void accelerate_dynamic(float acc_x, float acc_y, circle_object* circle) {
//  circle->x_acc += acc_x;
//  circle->y_acc += acc_y;
//}
//
//void set_new_velocity_dynamic(float x_pos, float y_pos, float dt,
//                              circle_object* circle) {
//  circle->x_prev = circle->x - (x_pos * dt);
//  circle->y_prev = circle->y - (y_pos * dt);
//}
//
//void add_velocity_dynamic(float x_pos, float y_pos, float dt,
//                          circle_object* circle) {
//  circle->x_prev -= x_pos * dt;
//  circle->y_prev -= y_pos * dt;
//}
//
//float get_vel_x(float dt, circle_object* circle) {
//  return (circle->x - circle->x_prev) / dt;
//}
//
//float get_vel_y(float dt, circle_object* circle) {
//  return (circle->y - circle->y_prev) / dt;
//}
//
//void update_all() {
//  update_gravity();
//  resolve_collisions_dynamic();
//  check_bounds(270,319,0,239);
//  check_bounds(0,320,0,240);
//  for (int i = 0; i < MAX_CIRCLES; i++) {
//    circle_object* temp_circ = &circles[i];
//    if (temp_circ->prev_x_acc != 0) {
//      accelerate_dynamic(temp_circ->prev_x_acc, 0, temp_circ);
//    }
//    update_dynamic(0.2, temp_circ);
//    draw_circle(temp_circ->x, temp_circ->y, 9, circle9, rgb(255, 174, 66));
//    check_stopped(i);
//  }
//   draw_line(319,239,270,239, rgb(255, 174, 66));
//   draw_line(270,239,270,200, rgb(255, 174, 66));
//   draw_line (319,200,319,239, rgb(255, 174, 66));
//}
//
//// return true if the object has stopped and modify x_end and y_end to the ending position of the circle
//void check_stopped(int index){
//      circle_object * temp = &circles[index];
//      if((temp->x == temp->x_prev) && temp->y == (temp->y_prev)){
//        temp->x_end = temp->x;
//        temp->y_end = temp->y;
//        temp->stopped = true;
//        *ledPtr += 1;
//      }
//      else{
//        temp->stopped = false;
//         *ledPtr += 1;
//      }
//    }
//
//
//// return true if all the balls are within the bounds
//// should only be called after all the balls are stopped
//bool check_game_status(int x_min,int x_max, int y_min, int y_max){
//      bool status = true;
//      for(int i = 1; i < MAX_CIRCLES; i++){
//        circle_object temp = circles[i];
//        bool temp_status;
//      if(temp.x_end > x_min && temp.x_end < x_max && temp.y_end > y_min && temp.y_end < y_max){
//        temp_status = true;
//      }
//      else{
//        temp_status = false;
//      }
//      status = status & temp_status;
//      }
//      return status;
//}
//
//
//int main(void) {
//  // location of the front buffer
//  volatile int* pixel_ctrl_ptr = (int*)0xFF203020;
//  volatile int* status_reg = (int*)0xFF20302C;
//
//
//  bool game_over = false;
//
//  int db = 16;
//
//
//  circles[0].radius = 9;
//  circles[0].x = 20;
//  circles[0].y = 10;
//  circles[0].x_acc = 0;
//  circles[0].y_acc = GRAVITY_CONST;
//  circles[0].x_prev = 15;
//  circles[0].y_prev = 10;
//  circles[0].prev_x_acc = 0;
//  num_objects++;
//  circles[1].radius = 9;
//  circles[1].x = 319;
//  circles[1].y = 10;
//  circles[1].x_acc = 0;
//  circles[1].y_acc = GRAVITY_CONST;
//  circles[1].x_prev = 320;
//  circles[1].y_prev = 10;
//  circles[1].prev_x_acc = 0;
//
//  num_objects++;
//
//  clear_screen();
//  *ledPtr = 0;
//
//  const int dt = 0.2;
//  // infinite loop
//  while (1) {
//    clear_screen();
//
//    update_all();
//
//    wait_for_vsync();
//
//    int i;
//    for(i = 1; i < MAX_CIRCLES; i++){
//      if(!circles[i].stopped){
//        break;
//      }
//    }
//    //i == MAX_CIRClES if all circles are stopped
//    if(i == MAX_CIRCLES){
//      if(check_game_status(270,319,200,270)){
//        *ledPtr = 1;
//      }
//      else{
//        *ledPtr = 2;
//      }
//    }
//
//    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
//  }
//  return 0;
//}
