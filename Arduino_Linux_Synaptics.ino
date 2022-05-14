#include "Synaptics.h"
#include "Arduino_Linux_Synaptics.h"

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
  device->set_mode(0x81); // "New" absolute mode
  Serial.print("read_modes: "); // this is done inside the c-tor
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
  static synaptics_data _private;
  device->read_data();
  psmouse _psmouse;
  _psmouse.dev = &Serial;
  _psmouse._private = &_private;
  for (int i=0; i<6; i++)
    _psmouse.packet[i] = device->data[i];
  synaptics_process_byte(&_psmouse);
  delay(20);
}
