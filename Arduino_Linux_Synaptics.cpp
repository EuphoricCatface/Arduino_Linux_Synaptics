#include <stdint.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef int8_t s8;

#include "Arduino.h"
#include "Arduino_Linux_Synaptics.h"

/**** Device configurations ****/
/* An arduino device doesn't need to make the configuration dynamic.
 * Look into the capability values and see which your device supports */
#define A_CAP_IMAGE_SENSOR false
#define A_CAP_EXTENDED true
#define A_CAP_MULTIFINGER false
#define A_CAP_ADV_GESTURE true
#define A_CAP_IMAGE_SENSOR false
#define A_CAP_PALMDETECT true
#define A_CAP_FOUR_BUTTON false
#define A_CAP_MIDDLE_BUTTON false
#define A_CAP_MULTI_BUTTON_NO 0
#define A_CAP_CLICKPAD false
#define A_CAP_EXT_BUTTONS_STICK false

#define A_HAS_AGM ((A_CAP_ADV_GESTURE) || (A_CAP_IMAGE_SENSOR))

#define A_MODEL_NEWABS true
#define A_ID_FULL 0x702

#define A_X_RES 0x42

static const bool cr48_profile_sensor = false;

/**** Kernel driver constants ****/
/*
 * The x/y limits are taken from the Synaptics TouchPad interfacing Guide,
 * section 2.3.2, which says that they should be valid regardless of the
 * actual size of the sensor.
 * Note that newer firmware allows querying device for maximum useable
 * coordinates.
 */
#define XMIN 0
#define XMAX 6143
#define YMIN 0
#define YMAX 6143
#define XMIN_NOMINAL 1472
#define XMAX_NOMINAL 5472
#define YMIN_NOMINAL 1408
#define YMAX_NOMINAL 4448

/* Size in bits of absolute position values reported by the hardware */
#define ABS_POS_BITS 13

/*
 * These values should represent the absolute maximum value that will
 * be reported for a positive position value. Some Synaptics firmware
 * uses this value to indicate a finger near the edge of the touchpad
 * whose precise position cannot be determined.
 *
 * At least one touchpad is known to report positions in excess of this
 * value which are actually negative values truncated to the 13-bit
 * reporting range. These values have never been observed to be lower
 * than 8184 (i.e. -8), so we treat all values greater than 8176 as
 * negative and any other value as positive.
 */
#define X_MAX_POSITIVE 8176
#define Y_MAX_POSITIVE 8176

/* maximum ABS_MT_POSITION displacement (in mm) */
#define DMAX 10

/*****************************************************************************
 *	Synaptics communications functions
 ****************************************************************************/

/*
 * Synaptics touchpads report the y coordinate from bottom to top, which is
 * opposite from what userspace expects.
 * This function is used to invert y before reporting.
 */
static int synaptics_invert_y(int y)
{
	return YMAX_NOMINAL + YMIN_NOMINAL - y;
}

/*****************************************************************************
 *	Functions to interpret the absolute mode packets
 *      - Retrieved from torvalds/Linux commit e4ce4d3a939d97bea045eafa13ad1195695f91ce
 ****************************************************************************/

static void synaptics_parse_agm(const u8 buf[],
				struct synaptics_data *priv,
				struct synaptics_hw_state *hw)
{
	struct synaptics_hw_state *agm = &priv->agm;
	int agm_packet_type;

	agm_packet_type = (buf[5] & 0x30) >> 4;
	switch (agm_packet_type) {
	case 1:
		/* Gesture packet: (x, y, z) half resolution */
		agm->w = hw->w;
		agm->x = (((buf[4] & 0x0f) << 8) | buf[1]) << 1;
		agm->y = (((buf[4] & 0xf0) << 4) | buf[2]) << 1;
		agm->z = ((buf[3] & 0x30) | (buf[5] & 0x0f)) << 1;
		break;

	case 2:
		/* AGM-CONTACT packet: we are only interested in the count */
		priv->agm_count = buf[1];
		break;

	default:
		break;
	}
}

static void synaptics_parse_ext_buttons(const u8 buf[],
					struct synaptics_data *priv,
					struct synaptics_hw_state *hw)
{
	unsigned int ext_bits =
		(A_CAP_MULTI_BUTTON_NO + 1) >> 1;
	// unsigned int ext_mask = GENMASK(ext_bits - 1, 0);
	unsigned int ext_mask = (1 << ext_bits) - 1;

	hw->ext_buttons = buf[4] & ext_mask;
	hw->ext_buttons |= (buf[5] & ext_mask) << ext_bits;
}

static int synaptics_parse_hw_state(const u8 buf[],
				    struct synaptics_data *priv,
				    struct synaptics_hw_state *hw)
{
	memset(hw, 0, sizeof(struct synaptics_hw_state));

	if (A_MODEL_NEWABS) {
		hw->w = (((buf[0] & 0x30) >> 2) |
			 ((buf[0] & 0x04) >> 1) |
			 ((buf[3] & 0x04) >> 2));

		if (A_HAS_AGM && hw->w == 2) {
			synaptics_parse_agm(buf, priv, hw);
			return 1;
		}

		hw->x = (((buf[3] & 0x10) << 8) |
			 ((buf[1] & 0x0f) << 8) |
			 buf[4]);
		hw->y = (((buf[3] & 0x20) << 7) |
			 ((buf[1] & 0xf0) << 4) |
			 buf[5]);
		hw->z = buf[2];

		hw->left  = (buf[0] & 0x01) ? 1 : 0;
		hw->right = (buf[0] & 0x02) ? 1 : 0;

		if (priv->is_forcepad) {
			/*
			 * ForcePads, like Clickpads, use middle button
			 * bits to report primary button clicks.
			 * Unfortunately they report primary button not
			 * only when user presses on the pad above certain
			 * threshold, but also when there are more than one
			 * finger on the touchpad, which interferes with
			 * out multi-finger gestures.
			 */
			if (hw->z == 0) {
				/* No contacts */
				priv->press = priv->report_press = false;
			} else if (hw->w >= 4 && ((buf[0] ^ buf[3]) & 0x01)) {
				/*
				 * Single-finger touch with pressure above
				 * the threshold. If pressure stays long
				 * enough, we'll start reporting primary
				 * button. We rely on the device continuing
				 * sending data even if finger does not
				 * move.
				 */
				if  (!priv->press) {
					//priv->press_start = jiffies;
					priv->press_start = micros();
					priv->press = true;
				//} else if (time_after(jiffies,
				//		priv->press_start +
				//			msecs_to_jiffies(50))) {
				} else if (micros() > priv->press_start + 50) {
					priv->report_press = true;
				}
			} else {
				priv->press = false;
			}

			hw->left = priv->report_press;

		} else if (A_CAP_CLICKPAD) {
			/*
			 * Clickpad's button is transmitted as middle button,
			 * however, since it is primary button, we will report
			 * it as BTN_LEFT.
			 */
			hw->left = ((buf[0] ^ buf[3]) & 0x01) ? 1 : 0;

		} else if (A_CAP_MIDDLE_BUTTON) {
			hw->middle = ((buf[0] ^ buf[3]) & 0x01) ? 1 : 0;
			if (hw->w == 2)
				hw->scroll = (s8)buf[1];
		}

		if (A_CAP_FOUR_BUTTON) {
			hw->up   = ((buf[0] ^ buf[3]) & 0x01) ? 1 : 0;
			hw->down = ((buf[0] ^ buf[3]) & 0x02) ? 1 : 0;
		}

		if (A_CAP_MULTI_BUTTON_NO > 0 &&
		    ((buf[0] ^ buf[3]) & 0x02)) {
			synaptics_parse_ext_buttons(buf, priv, hw);
		}
	} else {
		hw->x = (((buf[1] & 0x1f) << 8) | buf[2]);
		hw->y = (((buf[4] & 0x1f) << 8) | buf[5]);

		hw->z = (((buf[0] & 0x30) << 2) | (buf[3] & 0x3F));
		hw->w = (((buf[1] & 0x80) >> 4) | ((buf[0] & 0x04) >> 1));

		hw->left  = (buf[0] & 0x01) ? 1 : 0;
		hw->right = (buf[0] & 0x02) ? 1 : 0;
	}

	/*
	 * Convert wrap-around values to negative. (X|Y)_MAX_POSITIVE
	 * is used by some firmware to indicate a finger at the edge of
	 * the touchpad whose precise position cannot be determined, so
	 * convert these values to the maximum axis value.
	 */
	if (hw->x > X_MAX_POSITIVE)
		hw->x -= 1 << ABS_POS_BITS;
	else if (hw->x == X_MAX_POSITIVE)
		hw->x = XMAX;

	if (hw->y > Y_MAX_POSITIVE)
		hw->y -= 1 << ABS_POS_BITS;
	else if (hw->y == Y_MAX_POSITIVE)
		hw->y = YMAX;

	return 0;
}

static void synaptics_report_semi_mt_slot(struct input_dev *dev, int slot,
					  bool active, int x, int y)
{
	input_mt_slot(dev, slot);
	input_mt_report_slot_state(dev, MT_TOOL_FINGER, active);
	if (active) {
		input_report_abs(dev, ABS_MT_POSITION_X, x);
		input_report_abs(dev, ABS_MT_POSITION_Y, synaptics_invert_y(y));
	}
}

static void synaptics_report_semi_mt_data(struct input_dev *dev,
					  const struct synaptics_hw_state *a,
					  const struct synaptics_hw_state *b,
					  int num_fingers)
{
	if (num_fingers >= 2) {
		synaptics_report_semi_mt_slot(dev, 0, true, min(a->x, b->x),
					      min(a->y, b->y));
		synaptics_report_semi_mt_slot(dev, 1, true, max(a->x, b->x),
					      max(a->y, b->y));
	} else if (num_fingers == 1) {
		synaptics_report_semi_mt_slot(dev, 0, true, a->x, a->y);
		synaptics_report_semi_mt_slot(dev, 1, false, 0, 0);
	} else {
		synaptics_report_semi_mt_slot(dev, 0, false, 0, 0);
		synaptics_report_semi_mt_slot(dev, 1, false, 0, 0);
	}
}

static void synaptics_report_ext_buttons(struct psmouse *psmouse,
					 const struct synaptics_hw_state *hw)
{
	struct input_dev *dev = psmouse->dev;
	struct synaptics_data *priv = psmouse->private;
	int ext_bits = (A_CAP_MULTI_BUTTON_NO + 1) >> 1;
	int i;

	if (!A_CAP_MULTI_BUTTON_NO)
		return;

	/* Bug in FW 8.1 & 8.2, buttons are reported only when ExtBit is 1 */
	if ((A_ID_FULL == 0x801 ||
	     A_ID_FULL == 0x802) &&
	    !((psmouse->packet[0] ^ psmouse->packet[3]) & 0x02))
		return;

	if (!A_CAP_EXT_BUTTONS_STICK) {
		for (i = 0; i < ext_bits; i++) {
			input_report_key(dev, BTN_0 + 2 * i,
				hw->ext_buttons & (1 << i));
			input_report_key(dev, BTN_1 + 2 * i,
				hw->ext_buttons & (1 << (i + ext_bits)));
		}
		return;
	}
}

static void synaptics_report_buttons(struct psmouse *psmouse,
				     const struct synaptics_hw_state *hw)
{
	struct input_dev *dev = psmouse->dev;
	struct synaptics_data *priv = psmouse->private;

	input_report_key(dev, BTN_LEFT, hw->left);
	input_report_key(dev, BTN_RIGHT, hw->right);

	if (A_CAP_MIDDLE_BUTTON)
		input_report_key(dev, BTN_MIDDLE, hw->middle);

	if (A_CAP_FOUR_BUTTON) {
		input_report_key(dev, BTN_FORWARD, hw->up);
		input_report_key(dev, BTN_BACK, hw->down);
	}

	synaptics_report_ext_buttons(psmouse, hw);
}

static void synaptics_report_mt_data(struct psmouse *psmouse,
				     const struct synaptics_hw_state *sgm,
				     int num_fingers)
{
	struct input_dev *dev = psmouse->dev;
	struct synaptics_data *priv = psmouse->private;
	const struct synaptics_hw_state *hw[2] = { sgm, &priv->agm };
	struct input_mt_pos pos[2];
	int slot[2], nsemi, i;

	nsemi = constrain(num_fingers, 0, 2);

	for (i = 0; i < nsemi; i++) {
		pos[i].x = hw[i]->x;
		pos[i].y = synaptics_invert_y(hw[i]->y);
	}

	input_mt_assign_slots(dev, slot, pos, nsemi, DMAX * A_X_RES);

	for (i = 0; i < nsemi; i++) {
		input_mt_slot(dev, slot[i]);
		input_mt_report_slot_state(dev, MT_TOOL_FINGER, true);
		input_report_abs(dev, ABS_MT_POSITION_X, pos[i].x);
		input_report_abs(dev, ABS_MT_POSITION_Y, pos[i].y);
		input_report_abs(dev, ABS_MT_PRESSURE, hw[i]->z);
	}

	input_mt_drop_unused(dev);

	/* Don't use active slot count to generate BTN_TOOL events. */
	input_mt_report_pointer_emulation(dev, false);

	/* Send the number of fingers reported by touchpad itself. */
	input_mt_report_finger_count(dev, num_fingers);

	synaptics_report_buttons(psmouse, sgm);

	input_sync(dev);
}

static void synaptics_image_sensor_process(struct psmouse *psmouse,
					   struct synaptics_hw_state *sgm)
{
	struct synaptics_data *priv = psmouse->private;
	int num_fingers;

	/*
	 * Update mt_state using the new finger count and current mt_state.
	 */
	if (sgm->z == 0)
		num_fingers = 0;
	else if (sgm->w >= 4)
		num_fingers = 1;
	else if (sgm->w == 0)
		num_fingers = 2;
	else if (sgm->w == 1)
		num_fingers = priv->agm_count ? priv->agm_count : 3;
	else
		num_fingers = 4;

	/* Send resulting input events to user space */
	synaptics_report_mt_data(psmouse, sgm, num_fingers);
}

static bool synaptics_has_multifinger(struct synaptics_data *priv)
{
	if (A_CAP_MULTIFINGER)
		return true;

	/* Advanced gesture mode also sends multi finger data */
	return A_HAS_AGM;
}

/*
 *  called for each full received packet from the touchpad
 */
static void synaptics_process_packet(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	struct synaptics_data *priv = psmouse->private;
	struct synaptics_device_info *info = &priv->info;
	struct synaptics_hw_state hw;
	int num_fingers;
	int finger_width;

	if (synaptics_parse_hw_state(psmouse->packet, priv, &hw))
		return;

	if (A_CAP_IMAGE_SENSOR) {
		synaptics_image_sensor_process(psmouse, &hw);
		return;
	}

	if (hw.scroll) {
		priv->scroll += hw.scroll;

		while (priv->scroll >= 4) {
			input_report_key(dev, BTN_BACK, !hw.down);
			input_sync(dev);
			input_report_key(dev, BTN_BACK, hw.down);
			input_sync(dev);
			priv->scroll -= 4;
		}
		while (priv->scroll <= -4) {
			input_report_key(dev, BTN_FORWARD, !hw.up);
			input_sync(dev);
			input_report_key(dev, BTN_FORWARD, hw.up);
			input_sync(dev);
			priv->scroll += 4;
		}
		return;
	}

	if (hw.z > 0 && hw.x > 1) {
		num_fingers = 1;
		finger_width = 5;
		if (A_CAP_EXTENDED) {
			switch (hw.w) {
			case 0 ... 1:
				if (synaptics_has_multifinger(priv))
					num_fingers = hw.w + 2;
				break;
			case 2:
				/*
				 * SYN_MODEL_PEN(info->model_id): even if
				 * the device supports pen, we treat it as
				 * a single finger.
				 */
				break;
			case 4 ... 15:
				if (A_CAP_PALMDETECT)
					finger_width = hw.w;
				break;
			}
		}
	} else {
		num_fingers = 0;
		finger_width = 0;
	}

	if (cr48_profile_sensor) {
		synaptics_report_mt_data(psmouse, &hw, num_fingers);
		return;
	}

	if (A_CAP_ADV_GESTURE)
		synaptics_report_semi_mt_data(dev, &hw, &priv->agm,
					      num_fingers);

	/* Post events
	 * BTN_TOUCH has to be first as mousedev relies on it when doing
	 * absolute -> relative conversion
	 */
	if (hw.z > 30) input_report_key(dev, BTN_TOUCH, 1);
	if (hw.z < 25) input_report_key(dev, BTN_TOUCH, 0);

	if (num_fingers > 0) {
		input_report_abs(dev, ABS_X, hw.x);
		input_report_abs(dev, ABS_Y, synaptics_invert_y(hw.y));
	}
	input_report_abs(dev, ABS_PRESSURE, hw.z);

	if (A_CAP_PALMDETECT)
		input_report_abs(dev, ABS_TOOL_WIDTH, finger_width);

	input_report_key(dev, BTN_TOOL_FINGER, num_fingers == 1);
	if (synaptics_has_multifinger(priv)) {
		input_report_key(dev, BTN_TOOL_DOUBLETAP, num_fingers == 2);
		input_report_key(dev, BTN_TOOL_TRIPLETAP, num_fingers == 3);
	}

	synaptics_report_buttons(psmouse, &hw);

	input_sync(dev);
}

static psmouse_ret_t synaptics_process_byte(struct psmouse *psmouse)
{
	synaptics_process_packet(psmouse);
	return PSMOUSE_FULL_PACKET;
}

