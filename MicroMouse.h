
#include <class/hid/hid.h>
#include "EspUsbHost.h"
int mouse_x = 120;
int mouse_y = 67;
bool mouse_left_down = false;
bool last_mouse_left_down = false;

class MyEspUsbHost : public EspUsbHost {
public:
  void onMouseButtons(hid_mouse_report_t report, uint8_t last_buttons) override {
    mouse_left_down = (report.buttons & MOUSE_BUTTON_LEFT);
  }
  void onMouseMove(hid_mouse_report_t report) override {
    mouse_x += (int8_t)report.x;
    mouse_y += (int8_t)report.y;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x > 239) mouse_x = 239;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y > 134) mouse_y = 134;
  }
};