#include "Synaptics.h"
#include "Arduino_Linux_Synaptics.h"
#include "dummy_psmouse.h"

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
  Serial.println("Init done!");
  device->set_mode(0x81); // Absolute mode + disgest (= EW mode) + W mode
  device->set_agm();
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
  Serial.print("x: ");
  Serial.print(dev.abs_x);
  Serial.print(" y: ");
  Serial.print(dev.abs_y);
  Serial.print(" z: ");
  Serial.print(dev.abs_pressure);
  Serial.print(" w: ");
  Serial.print(dev.abs_tool_width);
  Serial.println();
  Serial.print("mt_0 ");
  Serial.print(dev.mt_slot[0].mt_tool_finger);
  Serial.print(" x: ");
  Serial.print(dev.mt_slot[0].abs_mt_position_x);
  Serial.print(" y: ");
  Serial.print(dev.mt_slot[0].abs_mt_position_y);
  Serial.print(" mt_1 ");
  Serial.print(dev.mt_slot[1].mt_tool_finger);
  Serial.print(" x: ");
  Serial.print(dev.mt_slot[1].abs_mt_position_x);
  Serial.print(" y: ");
  Serial.print(dev.mt_slot[1].abs_mt_position_y);
  Serial.println();
  Serial.print("left: ");
  Serial.print(dev.btn_left);
  Serial.print(" right: ");
  Serial.print(dev.btn_right);
  Serial.print(" fingers: ");
  uint8_t fingers = ((uint8_t)dev.btn_tool_tripletap << 2) |
                    ((uint8_t)dev.btn_tool_doubletap << 1) |
                    ((uint8_t)dev.btn_tool_finger);
  Serial.print(fingers);
  Serial.println();
  memset(&dev, 0, sizeof(dev));
  delay(20);
}
