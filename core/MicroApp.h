#pragma once
#include <Arduino.h>

// Forward declaration only — MicroApp holds a Window* pointer but does not
// need the full Window definition.
class Window;

class MicroApp {
public:
  Window* window  = nullptr;
  String  appName = "Unknown App";

  virtual ~MicroApp() {}
  virtual void onLaunch(String args) = 0;
  virtual void onClose() {}
};
