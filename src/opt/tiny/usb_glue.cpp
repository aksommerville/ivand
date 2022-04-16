/* usb_glue.cpp
 * Helpers to send serial data over the USB line.
 * You can read these from the device file on the Linux side.
 * Unclear if any of this setup is really necessary:
 *   https://playground.arduino.cc/Interfacing/LinuxTTY/
 *   stty -F /dev/ttyACM0 cs8 9600 ignbrk -brkint -icrnl -imaxbel -opost -onlcr -isig 
 *     -icanon -iexten -echo -echoe -echok -echoctl -echoke noflsh -ixon -crtscts 
 */
 
#include <Wire.h>
#include <HID.h>

static uint8_t usb_init=0;
 
extern "C" {
  void usb_begin();
  void usb_send(const void *v,int c);
  int usb_read(void *dst,int dsta);
  int usb_read_byte();
}
 
void usb_begin() {
  if (usb_init) return;
  Wire.begin();
  USBDevice.init();
  USBDevice.attach();
  SerialUSB.begin(9600);
  usb_init=1;
}

void usb_send(const void *v,int c) {
  if (!usb_init||!v||(c<1)) return;
  SerialUSB.write((char*)v,c);
}

int usb_read(void *dst,int dsta) {
  if (!usb_init) return -1;
  return SerialUSB.readBytes((char*)dst,dsta);
}

int usb_read_byte() {
  if (!usb_init) return -1;
  return SerialUSB.read();
}
