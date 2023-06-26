#ifdef __CONTROLLER_TEST
#include "curtains_controller.h"
#include "death_handler.h"
#include <iostream>
#include <string.h>

namespace esphome {
GlobalPreferences *global_preferences = new GlobalPreferences();
uint32_t millisValue = 0;
uint32_t millis() { return millisValue; }
};  // namespace esphome

using Button = esphome::button::Button;
using Display = esphome::lcd_pcf8574::PCF8574LCDDisplay;
using CurtainsController = esphome::curtains_controller::CurtainsController;
using Stepper = esphome::stepper::Stepper;

Button *open_tulle_button;
Button *close_tulle_button;
Button *open_curtains_button;
Button *close_curtains_button;
Button *set_button;
Button *mode_button;
Display *display;
Stepper *tulle_stepper;
Stepper *curtains_stepper;
CurtainsController *curtains_controller;

void loop();
void cleanup();

void setup() {
  open_tulle_button = new Button();
  close_tulle_button = new Button();
  open_curtains_button = new Button();
  close_curtains_button = new Button();
  set_button = new Button();
  mode_button = new Button();
  display = new Display();
  tulle_stepper = new Stepper();
  curtains_stepper = new Stepper();
  curtains_controller = new CurtainsController();
  curtains_controller->setTulleOpenButton(open_tulle_button);
  curtains_controller->setTulleCloseButton(close_tulle_button);
  curtains_controller->setCurtainsOpenButton(open_curtains_button);
  curtains_controller->setCurtainsCloseButton(close_curtains_button);
  curtains_controller->setSetButton(set_button);
  curtains_controller->setModeButton(mode_button);
  curtains_controller->setDisplay(display);
  curtains_controller->setTulleMotor(tulle_stepper);
  curtains_controller->setCurtainsMotor(curtains_stepper);

  curtains_controller->setup();
  loop();
}

class FailedTestException : public std::exception {
 public:
  FailedTestException(std::string reason) { this->reason = reason; }
  const char *what() const noexcept override { return reason.c_str(); }

 private:
  std::string reason;
};

#define assert_equals(expected, actual) _assert_equals(__FILE__, __LINE__, expected, actual);

void _assert_equals(std::string file, uint32_t lineNo, uint32_t expected, uint32_t actual) {
  if (expected != actual) {
    char buf[1000];
    std::snprintf(&buf[0], 999, "Expected: %d, actual: %d", expected, actual);
    throw stacktrace::stack_exception<FailedTestException>(std::string(buf));
  }
}

void _assert_equals(std::string file, uint32_t lineNo, std::string expected, std::string actual) {
  bool stringsEqual = strcmp(expected.c_str(), actual.c_str()) == 0;
  if (!stringsEqual) {
    char buf[1000];
    std::snprintf(&buf[0], 999, "Expected: %s, actual: %s", expected.c_str(), actual.c_str());
    throw stacktrace::stack_exception<FailedTestException>(std::string(buf));
  }
}

using test_method = std::function<void()>;

void test(std::string testName, test_method test) {
  printf("Running %s...\n", testName.c_str());
  try {
    setup();
    test();
  } catch (const std::exception &e) {
    std::cerr << "Test error:\n" << e.what() << '\n';
    cleanup();
  }
}

void _assert_equals(std::string file, uint32_t lineNo, uint32_t expected, uint32_t actual, std::string reason) {
  if (expected != actual) {
    char buf[1000];
    std::snprintf(&buf[0], 999, "Expected: %d, actual: %d", expected, actual);
    throw stacktrace::stack_exception<FailedTestException>(reason + ": " + std::string(buf));
  }
}

void cleanup() {
  delete curtains_controller;
  delete open_tulle_button;
  delete close_tulle_button;
  delete open_curtains_button;
  delete close_curtains_button;
  delete set_button;
  delete mode_button;
  delete display;
  delete tulle_stepper;
  delete curtains_stepper;
}

void loop() {
  for (int i = 0; i < 5; i++) {
    curtains_controller->loop();
  }
}

void test_startup() {
  assert_equals(1, display->clear_calls);
  assert_equals(1, display->print_calls);
  assert_equals(1, display->display_calls);
  assert_equals("Ready", display->text);
}

void test_open_tulle_already_open() {
  display->resetMock();
  open_tulle_button->press();
  loop();

  assert_equals(1, display->clear_calls);
  assert_equals(1, display->print_calls);
  assert_equals(1, display->display_calls);
  assert_equals("Tulle already open", display->text);
}

using CurtainState = esphome::curtains_controller::curtains_state::CurtainState;

void test_close_tulle() {
  display->resetMock();
  close_tulle_button->press();
  loop();

  assert_equals(1, display->clear_calls);
  z assert_equals(1, display->print_calls);
  assert_equals(1, display->display_calls);
  assert_equals("Tulle is closing, curtains is open", display->text);
  assert_equals(CurtainState::Closing, globalState.get_tulle_state());
  assert_equals(0, tulle_stepper->position);
  assert_equals(3000, tulle_stepper->target);

  tulle_stepper->done_moving();
  loop();
  assert_equals("Ready", display->text);
  assert_equals(3000, tulle_stepper->position);
  assert_equals(3000, tulle_stepper->target);
  assert_equals(CurtainState::Closed, globalState.tulleState);
}

int main() {
  Debug::DeathHandler dh;
  dh.set_append_pid(true);
  dh.set_color_output(true);
  dh.set_generate_core_dump(true);

  printf("Running tests\n");
  // test("test_startup", test_startup);
  // test("test_open_tulle_already_open", test_open_tulle_already_open);
  test("test_close_tulle", test_close_tulle);
  printf("All tests finished\n");
}

#endif