#ifndef _DUMMY_PSMOUSE_H
#define _DUMMY_PSMOUSE_H

struct psmouse {
	HardwareSerial * dev;
	void * _private;
	unsigned char packet[8];
};

#endif
