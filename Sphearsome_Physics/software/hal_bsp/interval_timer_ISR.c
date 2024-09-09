#include "globals.h"

extern volatile int timeout;
extern volatile int * interval_timer_ptr;
extern volatile int seq;
extern volatile int dir;
extern volatile int delay;
extern volatile int chaserDelay;

/*******************************************************************************
 * Interval timer interrupt service routine
 *
 * Shifts a PATTERN being displayed on the LCD character display. The shift
 * direction is determined by the external variable KEY_PRESSED.
 *
*******************************************************************************/
void interval_timer_ISR(struct alt_up_dev * up_dev, unsigned int id) {

    *(interval_timer_ptr) = 0; // clear the interrupt

    if (delay == 0) {
    	delay = chaserDelay;
		// enable the audio interrupt
		//alt_up_audio_enable_write_interrupt(up_dev->audio_dev);

		volatile int * HEX3_HEX0_ptr = (int *)HEX3_HEX0_BASE;
		volatile int * HEX5_HEX4_ptr = (int *)HEX5_HEX4_BASE;

		unsigned char hex_segs[] = {0, 0, 0, 0, 0, 0, 0, 0};
		int sequence[] = {
			0x01, 0x01, 0x01, 0x01, 0x20, 0x10,
			0x08, 0x08, 0x08, 0x08, 0x04, 0x02};
		int hex[] = {2, 3, 4, 5, 5, 5, 5, 4, 3, 2, 2, 2};

		if (dir == 0) {
			if (seq > 11) {
				seq = 0;
			}
			hex_segs[hex[seq]] = sequence[seq];
			(seq)++;
		} else {
			if (seq < 0) {
				seq = 11;
			}
			hex_segs[hex[seq]] = sequence[seq];
			(seq)--;
		}
		/* drive the hex displays - need to mask off the score bits first and or with the chaser */
		*(HEX3_HEX0_ptr) = ((*(HEX3_HEX0_ptr)& 0x7FFF) | *(int *)(hex_segs));
		*(HEX5_HEX4_ptr) = *(int *)(hex_segs + 4);
    } else {
    	delay--;
    }

    timeout               = 1; // set global variable

    return;
}

