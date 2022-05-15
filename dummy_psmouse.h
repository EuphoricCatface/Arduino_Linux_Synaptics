#ifndef _DUMMY_PSMOUSE_H
#define _DUMMY_PSMOUSE_H

#define MT_MAX 2

struct _mt_slot {
  bool mt_tool_finger;
  int abs_mt_position_x;
  int abs_mt_position_y;
};

struct a_input_dev {
  struct _mt_slot mt_slot[MT_MAX];
  bool    btn_touch;
  int     abs_x;
  int     abs_y;
  uint8_t abs_pressure;
  uint8_t abs_tool_width;
  bool    btn_tool_finger;
  bool    btn_tool_doubletap;
  bool    btn_tool_tripletap;
  bool    btn_left;
  bool    btn_right;
};

struct psmouse {
	a_input_dev * dev;
	void * _private;
	unsigned char packet[8];
};

#endif
