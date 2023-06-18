#include "curtains_controller.h"

namespace esphome {
namespace curtains_controller {

void CurtainsController::setup() {
  initSteps();
  buttonState = button_state::None;
  lastStepStarted = millis();
  currentStep = step_id::Initial;
  tulleOpenButton.add_on_state_callback([this](bool state) { this->onTulleOpenButtonStateChanged(state); });
  tulleCloseButton.add_on_state_callback([this](bool state) { this->onTulleCloseButtonStateChanged(state); });
  curtainsOpenButton.add_on_state_callback([this](bool state) { this->onCurtainsOpenButtonStateChanged(state); });
  curtainsCloseButton.add_on_state_callback([this](bool state) { this->onCurtainsCloseButtonStateChanged(state); });
  setButton.add_on_state_callback([this](bool state) { this->onSetButtonStateChanged(state); });
  modeButton.add_on_state_callback([this](bool state) { this->onModeButtonStateChanged(state); });
  restoreGlobalState();
}

void CurtainsController::loop() {
  unsigned long nowMillis = millis();
  unsigned long millisSinceStart = nowMillis - lastStepStarted;
  StepNode *currentStepNode = step(currentStep);
  auto nextStep = currentStepNode->nextStep(globalState, buttonState, millisSinceStart);
  if (nextStep == step_id::None) {
    String text = currentStepNode->getDisplayText(globalState, buttonState, millisSinceStart);
    printText(text);
    return;
  }
  currentStepNode->exit(globalState, buttonState, millisSinceStart);
  currentStep = nextStep;
  currentStepNode = step(currentStep);
  lastStepStarted = nowMillis;
  currentStepNode->enter(globalState, buttonState, 0);
}

void setUpInitialStep(StepNode *step) {
  step->setCustomNextStep(
      [](GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millisSInceStart) -> step_id::StepId {
        return globalState.setupCompleted ? step_id::Ready : step_id::CalibrateTulleOpen;
      }, 
      []() -> std::unordered_set<step_id::StepId>* {
        auto res = new std::unordered_set<step_id::StepId>();
        res += step_id::Ready;
        res += step_id::CalibrateTulleOpen;
        return res;
      });
}

void setUpReadyStep(StepNode *step) {}

void CurtainsController::initSteps() {
  registerStep(new StepNode(step_id::Initial));
  registerStep(new StepNode(step_id::Ready, "Ready"));
  registerStep(new StepNode(step_id::OpeningClosing));
  registerStep(new StepNode(step_id::AlreadyInMotion, "Tulle or Curtains are already in motion"));
  registerStep(new StepNode(step_id::Unsupported, "Unsupported"));
  registerStep(new StepNode(step_id::ModePressed, "Press SET withint 3 seconds to enter setup mode"));
  registerStep(new StepNode(step_id::CalibrateTulleOpen, "Use tulle open and close button to adjust tulle opening position"));
  registerStep(new StepNode(step_id::CalibrateTulleClosed, "Use tulle open and close button to adjust tulle closing position"));
  registerStep(
      new StepNode(step_id::CalibrateCurtainsOpen, "Use curtains open and close button to adjust curtains opening position"));
  registerStep(
      new StepNode(step_id::CalibrateCurtainsClosed, "Use curtains open and close button to adjust curtains closing position"));

  setUpInitialStep(step(step_id::Initial));
  //setupReadyStep(step(step_id::Ready));
}

void CurtainsController::registerStep(StepNode *step) {
  if (steps.size() < step->getStepId() + 1) {
    steps.resize(step->getStepId() + 1);
  }
  steps[step->getStepId()] = step;
}

};  // namespace curtains_controller
};  // namespace esphome