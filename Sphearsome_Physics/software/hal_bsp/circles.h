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

//// iterate through all objects and resolve any collisions between them
//void resolve_collisions_dynamic();
///*
// * Call all the necessary functions to apply forces on objects, update
// * posisions, and resolve collisions
// */
//void update_all();
//
//void update_gravity();
//
//void check_bounds(int x_lim_min, int x_lim_max, int y_lim_min, int y_lim_max);
//
//void check_stopped(int index);
//
//bool check_game_status(int x_lim1,int x_lim2, int y_lim1, int y_lim2);
//
//void accelerate_dynamic(float acc_x, float acc_y, circle_object* circle);
