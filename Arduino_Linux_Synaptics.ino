#include "Synaptics.h"
#include "Arduino_Linux_Synaptics.h"
#include "dummy_psmouse.h"

#include "Bluetooth.h"
Bluetooth bluetooth(115200, false, 0, 0);

Synaptics *device = nullptr;

void print_data(bool data_packet=false) {
  Serial.print(device->data[0], HEX);
  Serial.print(" ");
  Serial.print(device->data[1], HEX);
  Serial.print(" ");
  Serial.print(device->data[2], HEX);
  Serial.print(" ");
  if (!data_packet) {
    Serial.println();
    return;
  }
  Serial.print(device->data[3], HEX);
  Serial.print(" ");
  Serial.print(device->data[4], HEX);
  Serial.print(" ");
  Serial.println(device->data[5], HEX);
}

void setup(void){
  Serial.begin(115200);
  device = new Synaptics(3, 2);
  //Serial.println("Init done!");
  device->set_mode(0xc5); // Absolute mode + fast mode + DisGest (= EW mode) + W mode
  device->set_agm();
  /*
  Serial.print("read_modes: ");
  device->read_modes();
  print_data();
  Serial.print("identify: ");
  device->identify();
  print_data();
  Serial.print("read_capabilities: ");
  device->read_capabilities();
  print_data();
  Serial.print("read_ext_cap: ");
  device->read_ext_cap();
  print_data();
  Serial.print("read_ext_cap_0c: ");
  device->read_ext_cap_0c();
  print_data();
  Serial.print("read_modelid: ");
  device->read_modelid();
  print_data();
  Serial.print("read_min: ");
  device->read_min();
  print_data();
  Serial.print("read_max: ");
  device->read_max();
  print_data();
  Serial.print("read_res: ");
  device->read_res();
  print_data();
  Serial.print("sleep for 5 secs...");
  delay(5000);
  Serial.println("done!");
  */
  device->enable();
}

void loop(void){
  device->read_data();

  psmouse _psmouse;
  static synaptics_data _private;
  static a_input_dev dev;

  _psmouse.dev = &dev;
  _psmouse._private = &_private;
  for (int i=0; i<6; i++)
    _psmouse.packet[i] = device->data[i];

  synaptics_process_byte(&_psmouse);
  uint8_t fingers = ((uint8_t)dev.btn_tool_tripletap << 2) |
                    ((uint8_t)dev.btn_tool_doubletap << 1) |
                    ((uint8_t)dev.btn_tool_finger);
  // These do not get updated on release
  dev.btn_tool_tripletap = dev.btn_tool_doubletap = dev.btn_tool_finger = false;
  /*
  Serial.print("x: ");  Serial.print(dev.abs_x);  Serial.print(" y: ");  Serial.print(dev.abs_y);
  Serial.print(" z: ");  Serial.print(dev.abs_pressure);  Serial.print(" w: ");  Serial.print(dev.abs_tool_width);
  Serial.println();
  Serial.print("mt_0 ");  Serial.print(dev.mt_slot[0].mt_tool_finger);  Serial.print(" x: ");  Serial.print(dev.mt_slot[0].abs_mt_position_x);  Serial.print(" y: ");  Serial.print(dev.mt_slot[0].abs_mt_position_y);
  Serial.print(" mt_1 ");  Serial.print(dev.mt_slot[1].mt_tool_finger);  Serial.print(" x: ");  Serial.print(dev.mt_slot[1].abs_mt_position_x);  Serial.print(" y: ");  Serial.print(dev.mt_slot[1].abs_mt_position_y);
  Serial.println();
  Serial.print("left: ");  Serial.print(dev.btn_left);  Serial.print(" right: ");  Serial.print(dev.btn_right);  Serial.print(" fingers: ");  Serial.print(fingers);
  Serial.println();
  */
  
  /******** Packet construction ********/

  /* Buttons */
  static uint8_t old_btn = 0;
  uint8_t btn = 0;
  /* - Physical */
  uint8_t phy_btn = ((uint8_t)dev.btn_right << 1) |
                    ((uint8_t)dev.btn_left);
  btn = phy_btn;

  /* - Tap gestures */
  // Is eating 200ms of input tolerable?
  // -> No it's not.
  uint8_t gest_btn = 0;
  do {
    if (phy_btn) break; // do~while(0) as goto
    
    static int32_t tap_detect_end = -1;
    // Every tap will have tap_detect_end, even if the time expires.
    // static bool old_btn_touch = false; // for tap-drag detection
    if (tap_detect_end == -1) {
      if (!dev.btn_touch)
        break;
      tap_detect_end = millis() + 200;
      break;
    }
    if (!dev.btn_touch) {
      if (tap_detect_end > millis())
        gest_btn = 0x01;
      tap_detect_end = -1;
    }
  } while(0);

  if (gest_btn) btn = gest_btn;

  /* - cleanup */
  uint8_t btn_xor = old_btn ^ btn;
  old_btn = btn;
  
  /* pointer movement */
  int rel_x = 0, rel_y = 0;
  static int old_x = 0, old_y = 0;
  rel_x = dev.abs_x - old_x; old_x = dev.abs_x;
  rel_y = dev.abs_y - old_y; old_y = dev.abs_y;

  rel_x = rel_x >> 2; rel_y = rel_y >> 2;
  if (rel_x < 0) rel_x++;
  if (rel_y < 0) rel_y++;

  /* Cursor pose */
  static bool old_single_touch = false;
  int cur_dx = 0, cur_dy = 0;
  
  bool single_touch = (fingers == 1) && dev.btn_touch;
  if (single_touch) {
    if (!old_single_touch) {
      cur_dx = 0; cur_dy = 0;
    } else {
      cur_dx = rel_x; cur_dy = rel_y;
    }
  }
  old_single_touch = single_touch;

  /* Scroll */
  static bool old_double_touch = false;
  int scr_y = 0;

  bool double_touch = (fingers == 2);
  if (double_touch) {
    if (!old_double_touch) {
      scr_y = 0;
    } else {
      scr_y = rel_y;
    }
  }
  old_double_touch = double_touch;

  if (btn_xor || cur_dx || cur_dy || scr_y)
    bluetooth.sendMouseState(btn, cur_dx, cur_dy, scr_y);
}
