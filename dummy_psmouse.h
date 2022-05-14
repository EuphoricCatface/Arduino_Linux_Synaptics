#ifndef _DUMMY_PSMOUSE_H
#define _DUMMY_PSMOUSE_H

struct psmouse {
	void * dev;
	void * _private;
	unsigned char packet[8];
};

#endif
