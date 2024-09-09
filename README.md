# Lanlin He, Noah Monti - "Sphearsome Physics"
## April 8th, 2024 
## ECE243, University of Toronto

### Operation:

The objective of the game is to place the blue, user controlled ball in such a way that it directs the green, game controlled ball into the end zone.

To launch the program, open a command prompt and cd to the "demo_batch" folder, and enter "Test.bat" to run. When prompted, press q and the program will load onto the board.

On the home screen, select either level to begin playing.

The operation is the same for both levels: use the PS2 mouse to place your ball anywhere inside the green rectangle near the top of the screen - the game won't allow you to place it anywhere else. Press the left mouse button to commit to a placement.

If the green (target) ball is moved to the red rectangle, you win.

If you were unsuccessful at shunting the target ball into the end zone, pressing the right mouse button will reset the level, however this will count as an attempt, and increase the score by 1 - the goal is to have the lowest score.

In level 1, the starting location for the target ball is fixed, making this level easier.

In level 2, the starting location for the target ball is randomized, so having good luck helps here, as well as knowing which places on the ball cause it to move most effectively.

If you want to take a break, or simply want a nice background, clicking the "Auto" button in level 2 will allow the game to play itself until the "Auto" button is clicked again.

To change levels or move to the home screen, click the "Exit" button at any time.

If you want to re-read the instructions, the "Instructions" button take you to a screen that has the game description. To leave the game, click the "Quit" button.

### References:

- Many of our routines utilized sample code and routines from the Board Support Package (BSP) to help setup interrupts, and access the board's memory in an optimized fashion. Code that contains "alt_up" utilizes these routines, which were created by Altera.
- We used code from https://github.com/johnBuffer/VerletSFML as a framework to understand how Verlet Integration could be implemented.
- There are several files that needed to be included in the code submission because of the use of a custom Board Support Package that utilized the faster floating point hardware (float2), code that we wrote is under /software/hal-bsp and are the C source and header files, code in other folders are the work of Altera.
