#include "globals.h"
//#include "jazzplaza.h"

/* globals used for audio playback */
extern volatile int *audioArrayPosition;
extern volatile int audioArraySize;
extern const int jazzplaza[];

/*******************************************************************************
 * Audio - Interrupt Service Routine
 *
 * This interrupt service routine records or plays back audio, depending on
 * which type interrupt (read or write) is pending in the audio device.
 ******************************************************************************/
void audio_ISR(struct alt_up_dev * up_dev, unsigned int id) {

	if (alt_up_audio_write_interrupt_pending(
            up_dev->audio_dev)) // check for write interrupt
    {

		volatile int * audio_ptr = (int *)AUDIO_SUBSYSTEM_AUDIO_BASE;
		int fifospace = *(audio_ptr + 1); // read the audio port fifospace register
		if ((fifospace & 0x00FF0000) > BUF_THRESHOLD) {
			// Some room; keep feeding the FIFO buffers until they are full
			while ((fifospace & 0x00FF0000) && (*audioArrayPosition <= audioArraySize)) {
				// write to audio buffers from saved data array
				alt_up_audio_write_fifo (up_dev->audio_dev, &(jazzplaza[*audioArrayPosition]), 1, ALT_UP_AUDIO_RIGHT);
				alt_up_audio_write_fifo (up_dev->audio_dev, &(jazzplaza[*audioArrayPosition]), 1, ALT_UP_AUDIO_LEFT);
				// Increment audio array position and check if we're at the end of the array
				++(*audioArrayPosition);
				if (*audioArrayPosition >= audioArraySize) {
					// Loop sample
					*audioArrayPosition = 0;
				}
				// Check how much FIFO room is left
				fifospace = *(audio_ptr + 1); // read the audio port fifospace register
			}
		}
    }
	//alt_up_audio_disable_write_interrupt(up_dev->audio_dev);

    return;
}
