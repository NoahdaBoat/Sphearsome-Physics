#include "globals.h"

// Not sure if all of these need to be volatile
extern volatile unsigned char byte1, byte2, byte3;
extern volatile bool button1Down, button2Down, button3Down;
extern volatile short int mouseX, mouseY;
extern volatile int byteIndex; // packet order
extern volatile int packetSize;
extern volatile bool negX, negY;
extern volatile unsigned char negXMask;
extern volatile unsigned char negYMask;
extern volatile bool newData;

/*******************************************************************************
 * PS2 ISR
 *
 * Rejects as many bad packets as possible and stores valid 3-byte packets.
 * The first of three bytes is overflow, sign, and mouse button status.
 * The next two bytes are x and y acceleration.
 * Conveniently also stores mouse button status for left and right buttons.
 ******************************************************************************/
void PS2_ISR(struct alt_up_dev * up_dev, unsigned int id) {
    unsigned char PS2_data;

    // This routine will try to ensure reads are valid
    if (alt_up_ps2_read_data_byte(up_dev->PS2_dev, &PS2_data) == 0) {
    	/* 0xFA is likely an ACK - need to 'ignore' those */
    	/* Bit 3 of the first of 3 bytes must be set - exit process if not */
    	if (byteIndex < 0 && (!(PS2_data & 0x08) || PS2_data == 0xFA || PS2_data == 0xAA)) {
			newData = false;
    		return;
    	}
    	if (byteIndex < 0 && (PS2_data & 0x08)) {
			newData = false;
    		byteIndex = 0;
    	}
		switch (byteIndex) {
			case 0:
				// Check if the byte is valid - should be
				if (PS2_data & 0x08) {
					byte1 = PS2_data;
					button1Down = PS2_data & 0x01;
					button2Down = PS2_data & 0x02;
					button3Down = PS2_data & 0x04;
					negX = PS2_data & negXMask;
					negY = PS2_data & negYMask;
				} else {
					byteIndex = -1;  // Bytes are likely out of order - reset
					return;
				}
				break;
			case 1:
				byte2 = PS2_data;
				// Ensure the overflow bit isn't set
				if ((byte1 & 0x40) == 0) {
					// Need the two's complement x-coord
					if (negX) {
						mouseX = (short int)(PS2_data | 0xFF00);
					} else {
						mouseX = (short int)(PS2_data);
					}
				}
				break;
			case 2:
				byte3 = PS2_data;
				// Ensure the overflow bit isn't set
				if ((byte1 & 0x40) == 0) {
					// Need the two's complement y-coord
					if (negY) {
						mouseY = (short int)(PS2_data | 0xFF00)*-1;
					 } else {
						mouseY = (short int)(PS2_data)*-1;
					 }
				  }
				break;
			case 3:
				// Should not encounter this unless using an Intellimouse
				break;
		} // switch
		byteIndex++;
		if (byteIndex >= packetSize) {
			newData = true;
			byteIndex = 0;
		}
    }
    /* This remains just in case someone would like to detect when the mouse is plugged-in
     * if ((byte2 == (unsigned char)0xAA) && (byte3 == (unsigned char)0x00))
            // mouse inserted; initialize sending of data
            (void)alt_up_ps2_write_data_byte(up_dev->PS2_dev,
                                             (unsigned char)0xF4);
    }*/
    return;
}
