/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Synaptics TouchPad PS/2 mouse driver
 */

#ifndef _SYNAPTICS_H
#define _SYNAPTICS_H

#include <stdint.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef int8_t s8;

#include "input-event-codes.h"

/*
 * MT_TOOL types (from input.h of Linux)
 */
#define MT_TOOL_FINGER    0x00
#define MT_TOOL_PEN   0x01
#define MT_TOOL_PALM    0x02
#define MT_TOOL_DIAL    0x0a
#define MT_TOOL_MAX   0x0f

#include "dummy_psmouse.h"

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

void synaptics_process_byte(struct psmouse *psmouse);

#endif /* _SYNAPTICS_H */
