/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Synaptics TouchPad PS/2 mouse driver
 */

#ifndef _SYNAPTICS_H
#define _SYNAPTICS_H

/*
 * A structure to describe the state of the touchpad hardware (buttons and pad)
 */
struct synaptics_hw_state {
	int x;
	int y;
	int z;
	int w;
	unsigned int left:1;
	unsigned int right:1;
	unsigned int middle:1;
	unsigned int up:1;
	unsigned int down:1;
	u8 ext_buttons;
	s8 scroll;
};

struct synaptics_data {
	u8 mode;				/* current mode byte */
	int scroll;

	bool absolute_mode;			/* run in Absolute mode */
	bool disable_gesture;			/* disable gestures */

	struct serio *pt_port;			/* Pass-through serio port */

	/*
	 * Last received Advanced Gesture Mode (AGM) packet. An AGM packet
	 * contains position data for a second contact, at half resolution.
	 */
	struct synaptics_hw_state agm;
	unsigned int agm_count;			/* finger count reported by agm */

	/* ForcePad handling */
	unsigned long				press_start;
	bool					press;
	bool					report_press;
	bool					is_forcepad;
};

#endif /* _SYNAPTICS_H */
