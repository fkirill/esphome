#pragma once
#ifdef __CONTROLLER_TEST
#include <unistd.h>
#include <functional>
#include <exception>
#include "stack_exception.hpp"

#define HOT __attribute__((hot))

#define USE_LOGGER

#define esph_log_i(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_INFO, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)
#define ESP_LOGI(tag, ...) esph_log_i(tag, __VA_ARGS__)

static const char *const LOG_LEVEL_LETTERS[] = {
    "",    // NONE
    "E",   // ERROR
    "W",   // WARNING
    "I",   // INFO
    "C",   // CONFIG
    "D",   // DEBUG
    "V",   // VERBOSE
    "VV",  // VERY_VERBOSE
};

void HOT esp_log_vprintf_(int level, const char *tag, int line, const char *format, va_list args);
void HOT esp_log_printf_(int level, const char *tag, int line, const char *format, ...);

#define ESPHOME_LOG_LEVEL_ERROR 1
#define ESPHOME_LOG_LEVEL_INFO 3
#define ESPHOME_LOG_LEVEL_CONFIG 4
#define ESPHOME_LOG_FORMAT(format) format
#define esph_log_config(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_CONFIG, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)
#define esph_log_i(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_INFO, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)
#define esph_log_e(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_ERROR, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)

#define ESP_LOGCONFIG(tag, ...) esph_log_config(tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) esph_log_i(tag, __VA_ARGS__)
#define ESP_LOGE(tag, ...) esph_log_e(tag, __VA_ARGS__)

#define SCHEDULER_DONT_RUN -1
#define COMPONENT_STATE_MASK 0xff;
#define COMPONENT_STATE_LOOP 2
#define COMPONENT_STATE_SETUP 1

namespace esphome {
class Component {
 public:
  virtual void setup() {}
  virtual void loop() {}
  void mark_failed() {}
  bool is_failed() { return false; }
  uint32_t get_component_state() { return COMPONENT_STATE_LOOP; }
};
namespace output {
class FloatOutput {};
};  // namespace output
namespace stepper {
class Stepper : public Component {
 public:
  void set_target(int32_t target) { this->target = target; }
  uint32_t get_target() { return target; }
  uint32_t get_position() { return position; }
  bool has_reached_target() { return position == target; }
  void report_position(int32_t position) { this->position = position; }

  void done_moving() { position = target; }

  uint32_t target = 0;
  uint32_t position = 0;
};
};  // namespace stepper
namespace button {
class Button : public Component {
 public:
  void add_on_press_callback(std::function<void()> &&callback) { this->callback = callback; }
  void press() {
    if (callback != NULL) {
      callback();
    } else {
      printf("CALLBACK IS NULL!!!!\n");
    }
  }

 private:
  std::function<void()> callback = NULL;
};
};  // namespace button
namespace lcd_pcf8574 {
class PCF8574LCDDisplay : public Component {
 public:
  PCF8574LCDDisplay() { resetMock(); }
  void no_backlight() { no_backlight_calls++; }
  void backlight() { backlight_calls++; }
  void clear() { clear_calls++; }
  void set_update_interval(uint32_t interval) {
    set_update_interval_calls++;
    update_interval = interval;
  }
  void print(const char *str) {
    print_calls++;
    text = str;
  }
  void display() { display_calls++; }

  //----------

  void resetMock() {
    text = "";
    backlight_calls = 0;
    no_backlight_calls = 0;
    clear_calls = 0;
    set_update_interval_calls = 0;
    update_interval = 0;
    print_calls = 0;
    display_calls = 0;
  }

  std::string text;
  uint32_t backlight_calls;
  uint32_t no_backlight_calls;
  uint32_t clear_calls;
  uint32_t set_update_interval_calls;
  uint32_t update_interval;
  uint32_t print_calls;
  uint32_t display_calls;
};
}  // namespace lcd_pcf8574
class ESPPreferenceObject {};
class GlobalPreferences {
 public:
  ESPPreferenceObject make_preference(size_t size, uint32_t hash, bool store) {
    auto pref = new ESPPreferenceObject();
    return *pref;
  }
};
extern GlobalPreferences *global_preferences;

extern uint32_t millisValue;
uint32_t millis();
};  // namespace esphome

#endif