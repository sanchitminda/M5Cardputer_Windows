#pragma once
#include <Arduino.h>
#include <functional>

// FreeRTOS bridge: runs a std::function lambda as a FreeRTOS task.
// Must be inline (not static) so multiple translation units share one definition.
inline void _appTaskWrapper(void* parameter) {
  auto* taskLogic = static_cast<std::function<void()>*>(parameter);
  (*taskLogic)();
  delete taskLogic;
  vTaskDelete(NULL);
}
