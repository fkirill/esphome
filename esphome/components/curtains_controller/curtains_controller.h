#pragma once

#include <unordered_set>
#include <string>

#ifdef __CONTROLLER_TEST
#include "test.h"
#else
#include "esphome/core/component.h"
#include "esphome/components/stepper/stepper.h"
#include "esphome/components/button/button.h"
#include "esphome/components/lcd_pcf8574/pcf8574_display.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"
#endif
#include "esphome/core/log.h"

static const char *const TAG = "curtains_controller";
namespace esphome {
namespace curtains_controller {

namespace curtains_state {
enum CurtainState { Opening, Open, Closing, Closed };
};

class GlobalState {
 public:
  uint32_t get_tulle_open_position() { return tulleOpenPosition; }
  uint32_t get_tulle_close_position() { return tulleClosePosition; }
  uint32_t get_curtains_open_position() { return curtainsOpenPosition; }
  uint32_t get_curtains_close_position() { return curtainsClosePosition; }
  curtains_state::CurtainState get_tulle_state() { return tulleState; }
  curtains_state::CurtainState get_curtains_state() { return curtainsState; }
  bool is_setup_completed() { return setupCompleted; }
  void set_tulle_open_position(uint32_t position) { tulleOpenPosition = position; }
  void set_tulle_close_position(uint32_t position) { tulleClosePosition = position; }
  void set_curtains_open_position(uint32_t position) { curtainsOpenPosition = position; }
  void set_curtains_close_position(uint32_t position) { curtainsClosePosition = position; }
  void set_tulle_state(curtains_state::CurtainState state) { tulleState = state; }
  void set_curtains_state(curtains_state::CurtainState state) { curtainsState = state; }
  void set_setup_completed(bool setup_completed) { setupCompleted = setup_completed; }

 private:
  uint32_t tulleOpenPosition;
  uint32_t tulleClosePosition;
  uint32_t curtainsOpenPosition;
  uint32_t curtainsClosePosition;
  curtains_state::CurtainState tulleState;
  curtains_state::CurtainState curtainsState;
  bool setupCompleted;
};

namespace button_state {
enum ButtonState { None, TulleOpen, TulleClose, CurtainsOpen, CurtainsClose, Set, Mode };
};

namespace step_id {
// no not assign manual values, we rely on the automatic value assignment by the compiler
enum StepId {
  Initial,  // decide wheater to go to the setup or to the Ready
  Ready,
  OpenTulle,
  CloseTulle,
  OpenCurtains,
  CloseCurtains,
  DoOpenTulle,
  DoCloseTulle,
  DoOpenCurtains,
  DoCloseCurtains,
  TulleAlreadyOpen,
  TulleAlreadyClosed,
  CurtainsAlreadyOpen,
  CurtainsAlreadyClosed,
  ReadySetUnsupported,
  OpeningClosing,
  AlreadyInMotion,
  ModePressed,
  CalibrateTulleOpen,
  CalibrateTulleOpenStartAdjustOpen,
  CalibrateTulleOpenAdjustOpen,
  CalibrateTulleOpenStartAdjustClose,
  CalibrateTulleOpenAdjustClose,
  CalibrateTulleClose,
  CalibrateTulleCloseStartAdjustOpen,
  CalibrateTulleCloseAdjustOpen,
  CalibrateTulleCloseStartAdjustClose,
  CalibrateTulleCloseAdjustClose,
  CalibrateCurtainsOpen,
  CalibrateCurtainsOpenStartAdjustOpen,
  CalibrateCurtainsOpenAdjustOpen,
  CalibrateCurtainsOpenStartAdjustClose,
  CalibrateCurtainsOpenAdjustClose,
  CalibrateCurtainsClose,
  CalibrateCurtainsCloseStartAdjustOpen,
  CalibrateCurtainsCloseAdjustOpen,
  CalibrateCurtainsCloseStartAdjustClose,
  CalibrateCurtainsCloseAdjustClose,
  SetupComplete,
  None,  // Special value, means NULL/nothing/no change.
};

};  // namespace step_id

typedef std::function<step_id::StepId(GlobalState &, button_state::ButtonState, uint32_t)> customNextStepFn;
typedef std::function<std::unordered_set<step_id::StepId>()> customGetNextStepsFn;
typedef std::function<std::string(GlobalState &, button_state::ButtonState, uint32_t)> customDislayTextFn;
typedef std::function<void(GlobalState &, button_state::ButtonState, uint32_t)> enterExitFn;

class TimeoutAndNextStep {
 public:
  TimeoutAndNextStep(uint32_t timeoutMillis, step_id::StepId nextStep)
      : timeoutMillis(timeoutMillis), nextStep(nextStep) {}

  const uint32_t timeoutMillis;
  const step_id::StepId nextStep;
};

class StepNode {
 public:
  StepNode(step_id::StepId stepId)
      : StepNode(stepId, std::string("<Undefined for ") + std::to_string(stepId) + std::string(">")) {}

  StepNode(step_id::StepId stepId, std::string displayText) {
    this->stepId = stepId;
    this->displayText = displayText;
  }

  step_id::StepId getStepId() { return stepId; }

  std::string getDisplayText() { return displayText; }

  StepNode *onTulleOpen(step_id::StepId stepId) {
    tulleOpenStep = stepId;
    return this;
  }

  StepNode *onTulleClose(step_id::StepId stepId) {
    tulleCloseStep = stepId;
    return this;
  }

  StepNode *onCurtainsOpen(step_id::StepId stepId) {
    curtainsOpenStep = stepId;
    return this;
  }

  StepNode *onCurtainsClose(step_id::StepId stepId) {
    curtainsCloseStep = stepId;
    return this;
  }

  StepNode *onSet(step_id::StepId stepId) {
    setStep = stepId;
    return this;
  }

  StepNode *onMode(step_id::StepId stepId) {
    modeStep = stepId;
    return this;
  }

  StepNode *setCustomNextStep(customNextStepFn customNextStep, customGetNextStepsFn customGetNextSteps) {
    this->customNextStep = customNextStep;
    this->customGetNextSteps = customGetNextSteps;
    return this;
  }

  StepNode *setCustomDisplayText(customDislayTextFn customDisplayText) {
    this->customDisplayText = customDisplayText;
    return this;
  }

  StepNode *setTimeout(uint32_t timeoutMillis, step_id::StepId nextStep) {
    timeoutStep = new TimeoutAndNextStep(timeoutMillis, nextStep);
    return this;
  }

  step_id::StepId nextStep(GlobalState &globalState, button_state::ButtonState buttonState,
                           uint32_t millis_since_start) {
    switch (buttonState) {
      case button_state::TulleOpen:
        return tulleOpenStep;
      case button_state::TulleClose:
        return tulleCloseStep;
      case button_state::CurtainsOpen:
        return curtainsOpenStep;
      case button_state::CurtainsClose:
        return curtainsCloseStep;
      case button_state::Set:
        return setStep;
      case button_state::Mode:
        return modeStep;
      case button_state::None:
        break;
    }
    if (customNextStep != NULL) {
      return customNextStep(globalState, buttonState, millis_since_start);
    }
    if (timeoutStep != NULL && timeoutStep->timeoutMillis <= millis_since_start) {
      return timeoutStep->nextStep;
    }
    return step_id::None;
  }

  std::string getDisplayText(GlobalState &globalState, button_state::ButtonState buttonState,
                             uint32_t millis_since_start) {
    if (customDisplayText != NULL) {
      return customDisplayText(globalState, buttonState, millis_since_start);
    }
    return displayText;
  }

  void enter(GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millis_since_start) {
    if (enterFn != NULL) {
      enterFn(globalState, buttonState, millis_since_start);
    }
  }

  void exit(GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millis_since_start) {
    if (exitFn != NULL) {
      exitFn(globalState, buttonState, millis_since_start);
    }
  }

  std::unordered_set<step_id::StepId> getAllOutboundSteps() {
    auto res = std::unordered_set<step_id::StepId>();
    if (tulleOpenStep != step_id::None) {
      res.insert(tulleOpenStep);
    }
    if (tulleCloseStep != step_id::None) {
      res.insert(tulleCloseStep);
    }
    if (curtainsOpenStep != step_id::None) {
      res.insert(curtainsOpenStep);
    }
    if (curtainsCloseStep != step_id::None) {
      res.insert(curtainsCloseStep);
    }
    if (setStep != step_id::None) {
      res.insert(setStep);
    }
    if (modeStep != step_id::None) {
      res.insert(modeStep);
    }
    if (timeoutStep != NULL) {
      res.insert(timeoutStep->nextStep);
    }
    if (customGetNextSteps != NULL) {
      auto customNextSteps = customGetNextSteps();
      for (auto i = customNextSteps.begin(); i != customNextSteps.end(); i++) {
        res.insert(*i);
      }
    }
    return res;
  }

  StepNode *onEnter(enterExitFn enterFn) {
    this->enterFn = enterFn;
    return this;
  }
  StepNode *onExit(enterExitFn exitFn) {
    this->exitFn = exitFn;
    return this;
  }

  void dumpConfig() {
    ESP_LOGCONFIG(TAG, "StepId = %d", stepId);
    ESP_LOGCONFIG(TAG, "    DisplayText = %s", displayText.c_str());
    ESP_LOGCONFIG(TAG, "    enterFn = %s", enterFn != NULL ? "present" : "NULL");
    ESP_LOGCONFIG(TAG, "    exitFn = %s", exitFn != NULL ? "present" : "NULL");
    ESP_LOGCONFIG(TAG, "    tulleOpenStep = %d", tulleOpenStep);
    ESP_LOGCONFIG(TAG, "    tulleCloseStep = %d", tulleCloseStep);
    ESP_LOGCONFIG(TAG, "    curtainsOpenStep = %d", curtainsOpenStep);
    ESP_LOGCONFIG(TAG, "    curtainsCloseStep = %d", curtainsCloseStep);
    ESP_LOGCONFIG(TAG, "    setStep = %d", setStep);
    ESP_LOGCONFIG(TAG, "    modeStep = %d", modeStep);
    ESP_LOGCONFIG(TAG, "    customDisplayText = %s", customDisplayText != NULL ? "present" : "NULL");
    ESP_LOGCONFIG(TAG, "    customNextStep = %s", customNextStep != NULL ? "present" : "NULL");
    ESP_LOGCONFIG(TAG, "    customGetNextSteps = %s", customGetNextSteps != NULL ? "present" : "NULL");
    if (timeoutStep != NULL) {
      ESP_LOGCONFIG(TAG, "    timeoutStep.timeoutMillis: %d, nextStep: %d", timeoutStep->timeoutMillis,
                    timeoutStep->nextStep);
    } else {
      ESP_LOGCONFIG(TAG, "    timeoutStep = NULL");
    }
  }

 private:
  step_id::StepId stepId;
  std::string displayText;
  enterExitFn enterFn = NULL;
  enterExitFn exitFn = NULL;
  step_id::StepId tulleOpenStep = step_id::None;
  step_id::StepId tulleCloseStep = step_id::None;
  step_id::StepId curtainsOpenStep = step_id::None;
  step_id::StepId curtainsCloseStep = step_id::None;
  step_id::StepId setStep = step_id::None;
  step_id::StepId modeStep = step_id::None;
  customDislayTextFn customDisplayText = NULL;
  customNextStepFn customNextStep = NULL;
  customGetNextStepsFn customGetNextSteps = NULL;
  TimeoutAndNextStep *timeoutStep = NULL;
};

class CurtainsController : public output::FloatOutput, public Component {
 public:
  CurtainsController() {}
  void setTulleMotor(stepper::Stepper *stepper) { this->tulleStepper = stepper; }
  void setCurtainsMotor(stepper::Stepper *stepper) { this->curtainsStepper = stepper; }
  void setTulleOpenButton(button::Button *button) { this->tulleOpenButton = button; }
  void setTulleCloseButton(button::Button *button) { this->tulleCloseButton = button; }
  void setCurtainsOpenButton(button::Button *button) { this->curtainsOpenButton = button; }
  void setCurtainsCloseButton(button::Button *button) { this->curtainsCloseButton = button; }
  void setSetButton(button::Button *button) { this->setButton = button; }
  void setModeButton(button::Button *button) { this->modeButton = button; }
  void setDisplay(lcd_pcf8574::PCF8574LCDDisplay *display) { this->display = display; }

  void setup() override;
  void loop() override;

  void write_state(float state) {}

 private:
  stepper::Stepper *tulleStepper = NULL;
  stepper::Stepper *curtainsStepper = NULL;
  button::Button *tulleOpenButton = NULL;
  button::Button *tulleCloseButton = NULL;
  button::Button *curtainsOpenButton = NULL;
  button::Button *curtainsCloseButton = NULL;
  button::Button *setButton = NULL;
  button::Button *modeButton = NULL;
  lcd_pcf8574::PCF8574LCDDisplay *display = NULL;
  button_state::ButtonState buttonState = button_state::None;
  step_id::StepId currentStep = step_id::Initial;
  std::vector<StepNode *> steps = std::vector<StepNode *>();
  GlobalState globalState;
  unsigned long lastStepStarted = millis();
  static const uint32_t hash = 696104820;
  ESPPreferenceObject globalStateStorage = global_preferences->make_preference(sizeof(GlobalState), hash, true);
  std::string lastPrintedMessage = "";
  bool displayTurnedOn = false;
  uint32_t displayTurnedAt = 0;

  void onTulleOpenButtonStateChanged() { buttonState = button_state::TulleOpen; }
  void onTulleCloseButtonStateChanged() { buttonState = button_state::TulleClose; }
  void onCurtainsOpenButtonStateChanged() { buttonState = button_state::CurtainsOpen; }
  void onCurtainsCloseButtonStateChanged() { buttonState = button_state::CurtainsClose; }
  void onSetButtonStateChanged() { buttonState = button_state::Set; }
  void onModeButtonStateChanged() { buttonState = button_state::Mode; }
  void registerStep(StepNode *step);
  void registerSteps(std::vector<StepNode *> steps);
  void restoreGlobalState();
  void backupGlobalState();
  void initSteps();
  StepNode *step(step_id::StepId stepId) { return steps[stepId]; }
  void printText(std::string text);
  bool verifyStepsConfiguration();
};
};  // namespace curtains_controller
};  // namespace esphome
