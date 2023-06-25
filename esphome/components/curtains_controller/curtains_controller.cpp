#include "curtains_controller.h"

namespace esphome {
namespace curtains_controller {

std::string curtainsStateToDisplayText(curtains_state::CurtainState state) {
  switch (state) {
    case curtains_state::Opening:
      return "is opening";
    case curtains_state::Open:
      return "is open";
    case curtains_state::Closing:
      return "is closing";
    case curtains_state::Closed:
      return "is closed";
  }
  return "unexpected";
}

StepNode *modePressedStep() {
  return (new StepNode(step_id::ModePressed, "Press SET within 3 seconds to enter calibration mode"))
      ->onSet(step_id::CalibrateTulleOpen)
      ->setTimeout(3000, step_id::Ready);
}

std::vector<StepNode *> openCloseSteps(step_id::StepId stepId, stepper::Stepper *stepper,
                                       std::function<uint32_t(GlobalState &)> targetPositionFn,
                                       std::function<curtains_state::CurtainState(GlobalState &)> getStateFn,
                                       std::function<void(GlobalState &, curtains_state::CurtainState)> setStateFn,
                                       curtains_state::CurtainState expectedState,
                                       curtains_state::CurtainState setState, step_id::StepId nextStepId,
                                       step_id::StepId nextNextStepId, step_id::StepId alreadyAtStateStepId,
                                       step_id::StepId alreadyInMotionStepId) {
  StepNode *considerOpeningClosingStep =
      (new StepNode(stepId))
          ->setCustomNextStep(
              [getStateFn, expectedState, nextStepId, alreadyInMotionStepId, alreadyAtStateStepId](
                  GlobalState &globalState, button_state::ButtonState button_state, uint32_t millisSinceStart) {
                auto currentState = getStateFn(globalState);
                ESP_LOGI(TAG, "State = %d", currentState);
                if (currentState == expectedState) {
                  ESP_LOGI(TAG, "Proceeding to open/close state");
                  return nextStepId;
                }
                if (currentState == curtains_state::Opening || currentState == curtains_state::Closing) {
                  ESP_LOGI(TAG, "On the move at the moment");
                  return alreadyInMotionStepId;
                }
                ESP_LOGI(TAG, "Already at the desired state");
                return alreadyAtStateStepId;
              },
              [nextStepId, alreadyInMotionStepId, alreadyAtStateStepId]() {
                auto res = std::unordered_set<step_id::StepId>();
                res.insert(nextStepId);
                res.insert(alreadyInMotionStepId);
                res.insert(alreadyAtStateStepId);
                return res;
              });
  StepNode *startOpeningClosingStep =
      (new StepNode(nextStepId))
          ->onEnter([getStateFn, expectedState, stepper, targetPositionFn, setStateFn, setState](
                        GlobalState &globalState, button_state::ButtonState button_state, uint32_t millisSinceStart) {
            auto currentState = getStateFn(globalState);
            if (currentState == expectedState) {
              stepper->set_target(targetPositionFn(globalState));
              setStateFn(globalState, setState);
            }
          })
          ->setCustomNextStep([nextNextStepId](GlobalState &gloablState, button_state::ButtonState buttonState,
                                               uint32_t millisSInceStart) { return nextNextStepId; },
                              [nextNextStepId]() {
                                auto res = std::unordered_set<step_id::StepId>();
                                res.insert(nextNextStepId);
                                return res;
                              });

  auto res = std::vector<StepNode *>();
  res.push_back(considerOpeningClosingStep);
  res.push_back(startOpeningClosingStep);
  return res;
}

StepNode *customRoutingOnlyStep(step_id::StepId stepId, customNextStepFn nextStepFn,
                                customGetNextStepsFn getNextStepsFn) {
  return (new StepNode(stepId))->setCustomNextStep(nextStepFn, getNextStepsFn);
}

StepNode *timeoutTextStep(step_id::StepId stepId, std::string displayText, uint32_t delayMillis,
                          step_id::StepId returnToStep) {
  return (new StepNode(stepId, displayText))->setTimeout(delayMillis, returnToStep);
}

StepNode *readyStep() {
  return (new StepNode(step_id::Ready, "Ready"))
      ->onTulleOpen(step_id::OpenTulle)
      ->onTulleClose(step_id::CloseTulle)
      ->onCurtainsOpen(step_id::OpenCurtains)
      ->onCurtainsClose(step_id::CloseCurtains)
      ->onMode(step_id::ModePressed)
      ->onSet(step_id::ReadySetUnsupported);
}

StepNode *openingClosingStep(stepper::Stepper *tulleStepper, stepper::Stepper *curtainsStepper) {
  return (new StepNode(step_id::OpeningClosing))
      ->onTulleOpen(step_id::OpenTulle)
      ->onTulleClose(step_id::CloseTulle)
      ->onCurtainsOpen(step_id::OpenCurtains)
      ->onCurtainsClose(step_id::CloseCurtains)
      ->setCustomNextStep(
          [tulleStepper, curtainsStepper](GlobalState &globalState, button_state::ButtonState buttonState,
                                          uint32_t millisSinceStart) {
            curtains_state::CurtainState tulleState = globalState.get_tulle_state();
            curtains_state::CurtainState curtainsState = globalState.get_curtains_state();
            if ((tulleState == curtains_state::Opening || tulleState == curtains_state::Closing) &&
                tulleStepper->has_reached_target()) {
              if (tulleState == curtains_state::Opening) {
                globalState.set_tulle_state(curtains_state::Open);
              }
              if (tulleState == curtains_state::Closing) {
                globalState.set_tulle_state(curtains_state::Closed);
              }
            }
            if ((curtainsState == curtains_state::Opening) ||
                (curtainsState == curtains_state::Closing) && curtainsStepper->has_reached_target()) {
              if (curtainsState == curtains_state::Opening) {
                globalState.set_curtains_state(curtains_state::Open);
              }
              if (curtainsState == curtains_state::Closing) {
                globalState.set_curtains_state(curtains_state::Closed);
              }
            }
            bool allMotorsStopped = tulleStepper->has_reached_target() && curtainsStepper->has_reached_target();
            return allMotorsStopped ? step_id::Ready : step_id::None;
          },
          []() {
            auto res = std::unordered_set<step_id::StepId>();
            res.insert(step_id::OpenTulle);
            res.insert(step_id::CloseTulle);
            res.insert(step_id::OpenCurtains);
            res.insert(step_id::CloseCurtains);
            res.insert(step_id::Ready);
            return res;
          })
      ->setCustomDisplayText(
          [](GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millisSinceStart) {
            return "Tulle " + curtainsStateToDisplayText(globalState.get_tulle_state()) + ", curtains " +
                   curtainsStateToDisplayText(globalState.get_curtains_state());
          });
}

bool CurtainsController::verifyStepsConfiguration() {
  auto definedSteps = std::unordered_set<step_id::StepId>();
  auto undefinedSteps = std::unordered_set<step_id::StepId>();
  auto referencedSteps = std::unordered_set<step_id::StepId>();
  for (auto i = 0; i < steps.size(); i++) {
    if (steps[i] != NULL) {
      definedSteps.insert(steps[i]->getStepId());
    } else {
      undefinedSteps.insert((step_id::StepId) i);
    }
  }
  for (auto i = undefinedSteps.begin(); i != undefinedSteps.end(); i++) {
    ESP_LOGE(TAG, "Step is not defined: %d", *i);
  }
  for (int i = 0; i < steps.size(); i++) {
    if (steps.at(i) != NULL) {
      auto outboundSteps = steps.at(i)->getAllOutboundSteps();
      for (auto iter = outboundSteps.begin(); iter != outboundSteps.end(); ++iter) {
        if (*iter != step_id::None) {
          ESP_LOGI(TAG, "Step dependency: %d/%d", i, *iter);
          referencedSteps.insert(*iter);
        }
      }
    }
  }
  bool res = true;
  for (auto iter = referencedSteps.begin(); iter != referencedSteps.end(); ++iter) {
    step_id::StepId stepId = *iter;
    bool foundMatch = false;
    for (auto iter2 = definedSteps.begin(); iter2 != definedSteps.end(); ++iter2) {
      if (*iter2 == stepId) {
        foundMatch = true;
        break;
      }
    }
    if (!foundMatch) {
      ESP_LOGE(TAG, "Step is being referred to, but is not defined: %d", stepId);
      res = false;
    }
  }
  if (!res) {
    ESP_LOGE(TAG, "Please see diagnostic step dump below");
    for (int i = 0; i < steps.size(); i++) {
      if (steps.at(i) != NULL) {
        steps.at(i)->dumpConfig();
      }
    }
  }
  return res;
}

std::vector<StepNode *> adjustSteps(step_id::StepId mainStepId, std::string mainDisplayText,
                                    step_id::StepId tulleOpenStepId, step_id::StepId tulleCloseStepId,
                                    step_id::StepId curtainsOpenStepId, step_id::StepId curtainsCloseStepId,
                                    step_id::StepId nextStepId, step_id::StepId startAdjustOpenStepId,
                                    step_id::StepId startAdjustCloseStepId, step_id::StepId adjustOpenStepId,
                                    step_id::StepId adjustCloseStepId, std::string adjustOpenDisplayText,
                                    std::string adjustCloseDisplayText, stepper::Stepper *stepper,
                                    std::function<uint32_t(GlobalState &)> positionGetter,
                                    std::function<void(GlobalState &, uint32_t)> positionSetter,
                                    uint32_t stepsIncrement) {
  auto mainStep = (new StepNode(mainStepId, mainDisplayText))
                      ->onTulleOpen(tulleOpenStepId)
                      ->onTulleClose(tulleCloseStepId)
                      ->onCurtainsOpen(curtainsOpenStepId)
                      ->onCurtainsClose(curtainsCloseStepId)
                      ->onSet(nextStepId);
  auto startAdjustOpenStep =
      (new StepNode(startAdjustOpenStepId))
          ->onEnter([stepper, positionSetter, stepsIncrement](
                        GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millisSinceStart) {
            stepper->report_position(stepsIncrement);
            stepper->set_target(0);
            positionSetter(globalState, 0);
          })
          ->setCustomNextStep([adjustOpenStepId](GlobalState &globalState, button_state::ButtonState buttonState,
                                                 uint32_t millisSinceStart) { return adjustOpenStepId; },
                              [adjustOpenStepId]() {
                                auto res = std::unordered_set<step_id::StepId>();
                                res.insert(adjustOpenStepId);
                                return res;
                              });
  auto startAdjustCloseStep =
      (new StepNode(startAdjustCloseStepId))
          ->onEnter([stepper, positionGetter, positionSetter, stepsIncrement](
                        GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millisSinceStart) {
            uint32_t curPosition = positionGetter(globalState);
            stepper->set_target(curPosition + stepsIncrement);
            positionSetter(globalState, curPosition + stepsIncrement);
          })
          ->setCustomNextStep([adjustCloseStepId](GlobalState &globalState, button_state::ButtonState buttonState,
                                                  uint32_t millisSinceStart) { return adjustCloseStepId; },
                              [adjustCloseStepId]() {
                                auto res = std::unordered_set<step_id::StepId>();
                                res.insert(adjustCloseStepId);
                                return res;
                              });
  auto adjustOpenStep = (new StepNode(adjustOpenStepId, adjustOpenDisplayText))
                            ->setCustomNextStep(
                                [stepper, mainStepId](GlobalState &globalState, button_state::ButtonState buttonState,
                                                      uint32_t millisSInceStart) {
                                  return stepper->has_reached_target() ? mainStepId : step_id::None;
                                },
                                [mainStepId]() {
                                  auto res = std::unordered_set<step_id::StepId>();
                                  res.insert(mainStepId);
                                  return res;
                                });
  auto adjustCloseStep = (new StepNode(adjustCloseStepId, adjustCloseDisplayText))
                             ->setCustomNextStep(
                                 [stepper, mainStepId](GlobalState &globalState, button_state::ButtonState buttonState,
                                                       uint32_t millisSInceStart) {
                                   return stepper->has_reached_target() ? mainStepId : step_id::None;
                                 },
                                 [mainStepId]() {
                                   auto res = std::unordered_set<step_id::StepId>();
                                   res.insert(mainStepId);
                                   return res;
                                 });

  auto steps = std::vector<StepNode *>();
  steps.push_back(mainStep);
  steps.push_back(startAdjustOpenStep);
  steps.push_back(startAdjustCloseStep);
  steps.push_back(adjustOpenStep);
  steps.push_back(adjustCloseStep);
  return steps;
}

void CurtainsController::setup() {
  initSteps();
  buttonState = button_state::None;
  lastStepStarted = millis();
  currentStep = step_id::Initial;
  tulleOpenButton->add_on_press_callback([this]() { this->onTulleOpenButtonStateChanged(); });
  tulleCloseButton->add_on_press_callback([this]() { this->onTulleCloseButtonStateChanged(); });
  curtainsOpenButton->add_on_press_callback([this]() { this->onCurtainsOpenButtonStateChanged(); });
  curtainsCloseButton->add_on_press_callback([this]() { this->onCurtainsCloseButtonStateChanged(); });
  setButton->add_on_press_callback([this]() { this->onSetButtonStateChanged(); });
  modeButton->add_on_press_callback([this]() { this->onModeButtonStateChanged(); });
  restoreGlobalState();
  display->set_update_interval(SCHEDULER_DONT_RUN);
  globalState.set_setup_completed(true);
  globalState.set_tulle_open_position(0);
  globalState.set_tulle_close_position(3000);
  globalState.set_curtains_open_position(0);
  globalState.set_curtains_close_position(3000);
  globalState.set_tulle_state(curtains_state::Open);
  globalState.set_curtains_state(curtains_state::Open);
  if (!verifyStepsConfiguration()) {
    ESP_LOGE(TAG, "Step verification failed, see previous log messages, marking the ocmponent as failed");
    mark_failed();
  }
}

void CurtainsController::loop() {
  unsigned long nowMillis = millis();
  if (displayTurnedOn && (nowMillis - displayTurnedAt) > 30000) {
    display->no_backlight();
  }
  unsigned long millisSinceStart = nowMillis - lastStepStarted;
  StepNode *currentStepNode = step(currentStep);
  if (currentStepNode == NULL) {
    ESP_LOGE(TAG, "currentStepNode == null, stepId = %d", currentStep);
  }
  auto nextStep = currentStepNode->nextStep(globalState, buttonState, millisSinceStart);
  buttonState = button_state::None;
  if (nextStep == step_id::None) {
    std::string text = currentStepNode->getDisplayText(globalState, buttonState, millisSinceStart);
    printText(text);
    return;
  }
  currentStepNode->exit(globalState, buttonState, millisSinceStart);
  currentStep = nextStep;
  currentStepNode = step(currentStep);
  if (currentStepNode == NULL) {
    ESP_LOGE(TAG, "nextStepNode == null, stepId = %d", currentStep);
  }
  lastStepStarted = nowMillis;
  currentStepNode->enter(globalState, buttonState, 0);
}

StepNode *initialStep() {
  return customRoutingOnlyStep(
      step_id::Initial,
      [](GlobalState &globalState, button_state::ButtonState button_state, uint32_t millisSinseStart) {
        return globalState.is_setup_completed() ? step_id::Ready : step_id::CalibrateTulleOpen;
      },
      []() {
        auto res = std::unordered_set<step_id::StepId>();
        res.insert(step_id::Ready);
        res.insert(step_id::CalibrateTulleOpen);
        return res;
      });
}

void CurtainsController::initSteps() {
  registerStep(initialStep());
  registerStep(readyStep());
  std::function<curtains_state::CurtainState(GlobalState &)> getTulleState = [](GlobalState &globalState) {
    return globalState.get_tulle_state();
  };
  std::function<curtains_state::CurtainState(GlobalState &)> getCurtainsState = [](GlobalState &globalState) {
    return globalState.get_curtains_state();
  };
  std::function<void(GlobalState &, curtains_state::CurtainState)> setTulleState =
      [](GlobalState &globalState, curtains_state::CurtainState state) { globalState.set_tulle_state(state); };
  std::function<void(GlobalState &, curtains_state::CurtainState)> setCurtainsState =
      [](GlobalState &globalState, curtains_state::CurtainState state) { globalState.set_curtains_state(state); };
  registerSteps(openCloseSteps(
      step_id::OpenTulle, tulleStepper, [](GlobalState &globalState) { return globalState.get_tulle_open_position(); },
      getTulleState, setTulleState, curtains_state::Closed, curtains_state::Opening, step_id::DoOpenTulle,
      step_id::OpeningClosing, step_id::TulleAlreadyOpen, step_id::AlreadyInMotion));
  registerStep(timeoutTextStep(step_id::TulleAlreadyOpen, "Tulle already open", 1000, step_id::OpeningClosing));
  registerSteps(openCloseSteps(
      step_id::CloseTulle, tulleStepper,
      [](GlobalState &globalState) { return globalState.get_tulle_close_position(); }, getTulleState, setTulleState,
      curtains_state::Open, curtains_state::Closing, step_id::DoCloseTulle, step_id::OpeningClosing,
      step_id::TulleAlreadyClosed, step_id::AlreadyInMotion));
  registerStep(timeoutTextStep(step_id::TulleAlreadyClosed, "Tulle already closed", 1000, step_id::OpeningClosing));
  registerSteps(openCloseSteps(
      step_id::OpenCurtains, curtainsStepper,
      [](GlobalState &globalState) { return globalState.get_curtains_open_position(); }, getCurtainsState,
      setCurtainsState, curtains_state::Closed, curtains_state::Opening, step_id::DoOpenCurtains,
      step_id::OpeningClosing, step_id::CurtainsAlreadyOpen, step_id::AlreadyInMotion));
  registerStep(timeoutTextStep(step_id::CurtainsAlreadyOpen, "Curtains already open", 1000, step_id::OpeningClosing));
  registerSteps(openCloseSteps(
      step_id::CloseCurtains, curtainsStepper,
      [](GlobalState &globalState) { return globalState.get_curtains_close_position(); }, getCurtainsState,
      setCurtainsState, curtains_state::Open, curtains_state::Closing, step_id::DoCloseCurtains,
      step_id::OpeningClosing, step_id::CurtainsAlreadyClosed, step_id::AlreadyInMotion));
  registerStep(
      timeoutTextStep(step_id::CurtainsAlreadyClosed, "Curtains already closed", 1000, step_id::OpeningClosing));
  registerStep(openingClosingStep(tulleStepper, curtainsStepper));
  registerStep(timeoutTextStep(step_id::AlreadyInMotion, "Tulle or Curtains are already in motion", 1000,
                               step_id::OpeningClosing));
  registerStep(timeoutTextStep(step_id::ReadySetUnsupported, "Press MODE before pressing SET", 1000, step_id::Ready));
  registerStep(modePressedStep());
  registerSteps(adjustSteps(
      step_id::CalibrateTulleOpen, "Use tulle open and close button to adjust tulle opening position",
      step_id::CalibrateTulleOpenStartAdjustOpen, step_id::CalibrateTulleOpenStartAdjustClose, step_id::None,
      step_id::None, step_id::CalibrateTulleClose, step_id::CalibrateTulleOpenStartAdjustOpen,
      step_id::CalibrateTulleOpenStartAdjustClose, step_id::CalibrateTulleOpenAdjustOpen,
      step_id::CalibrateTulleOpenAdjustClose, "Adjusting tulle open position, opening by 400 steps",
      "Adjusting tulle open position, closing by 400 steps", tulleStepper,
      [](GlobalState &globalState) { return globalState.get_tulle_open_position(); },
      [](GlobalState &globalState, uint32_t position) { globalState.set_tulle_close_position(position); }, 400));
  registerSteps(adjustSteps(
      step_id::CalibrateTulleClose, "Use tulle open and close button to adjust tulle closing position",
      step_id::CalibrateTulleCloseStartAdjustOpen, step_id::CalibrateTulleCloseStartAdjustClose, step_id::None,
      step_id::None, step_id::CalibrateCurtainsOpen, step_id::CalibrateTulleCloseStartAdjustOpen,
      step_id::CalibrateTulleCloseStartAdjustClose, step_id::CalibrateTulleCloseAdjustOpen,
      step_id::CalibrateTulleCloseAdjustClose, "Adjusting tulle close position, opening by 400 steps",
      "Adjusting tulle close position, closing by 400 steps", tulleStepper,
      [](GlobalState &globalState) { return globalState.get_tulle_close_position(); },
      [](GlobalState &globalState, uint32_t position) { globalState.set_tulle_close_position(position); }, 400));
  registerSteps(adjustSteps(
      step_id::CalibrateCurtainsOpen, "Use curtains open and close button to adjust curtains opening position",
      step_id::None, step_id::None, step_id::CalibrateCurtainsOpenStartAdjustOpen,
      step_id::CalibrateCurtainsOpenStartAdjustClose, step_id::CalibrateCurtainsClose,
      step_id::CalibrateCurtainsOpenStartAdjustOpen, step_id::CalibrateCurtainsOpenStartAdjustClose,
      step_id::CalibrateCurtainsOpenAdjustOpen, step_id::CalibrateCurtainsOpenAdjustClose,
      "Adjusting curtains open position, opening by 400 steps",
      "Adjusting curtains open position, closing by 400 steps", curtainsStepper,
      [](GlobalState &globalState) { return globalState.get_curtains_open_position(); },
      [](GlobalState &globalState, uint32_t position) { globalState.set_curtains_open_position(position); }, 400));
  registerSteps(adjustSteps(
      step_id::CalibrateCurtainsClose, "Use curtains open and close button to adjust curtains closing position",
      step_id::None, step_id::None, step_id::CalibrateCurtainsCloseStartAdjustOpen,
      step_id::CalibrateCurtainsCloseStartAdjustClose, step_id::SetupComplete,
      step_id::CalibrateCurtainsCloseStartAdjustOpen, step_id::CalibrateCurtainsCloseStartAdjustClose,
      step_id::CalibrateCurtainsCloseAdjustOpen, step_id::CalibrateCurtainsCloseAdjustClose,
      "Adjusting curtains close position, opening by 400 steps",
      "Adjusting curtains close position, closing by 400 steps", curtainsStepper,
      [](GlobalState &globalState) { return globalState.get_curtains_close_position(); },
      [](GlobalState &globalState, uint32_t position) { globalState.set_curtains_close_position(position); }, 400));

  registerStep(
      timeoutTextStep(step_id::SetupComplete, "Setup complete", 1000, step_id::Ready)
          ->onEnter([this](GlobalState &globalState, button_state::ButtonState buttonState, uint32_t millisSinceStart) {
            globalState.set_setup_completed(true);
            this->backupGlobalState();
          }));
}

void CurtainsController::registerStep(StepNode *step) {
  ESP_LOGI(TAG, "registering step address %u", step);
  ESP_LOGI(TAG, "registering step id %u", step->getStepId());

  if (steps.size() < step->getStepId() + 1) {
    steps.resize(step->getStepId() + 1);
  }
  steps[step->getStepId()] = step;
}

void CurtainsController::registerSteps(std::vector<StepNode *> stepsToRegister) {
  for (int i = 0; i < stepsToRegister.size(); i++) {
    registerStep(stepsToRegister.at(i));
  }
}

void CurtainsController::backupGlobalState() {  /*globalStateStorage.save(&globalState);*/
}
void CurtainsController::restoreGlobalState() { /*globalStateStorage.load(&globalState);*/
}

bool isDisplayReady(lcd_pcf8574::PCF8574LCDDisplay *display) {
  uint32_t state = display->get_component_state() & COMPONENT_STATE_MASK;
  return (state == COMPONENT_STATE_LOOP) || (state == COMPONENT_STATE_SETUP);
}

void CurtainsController::printText(std::string text) {
  bool isNewString = strcmp(text.c_str(), lastPrintedMessage.c_str()) != 0;
  if (isNewString && isDisplayReady(display)) {
    lastPrintedMessage = text;
    if (!display->is_failed()) {
      ESP_LOGI(TAG, "actually printing the text: %s", text.c_str());
      display->backlight();
      displayTurnedAt = millis();
      displayTurnedOn = true;
      display->clear();
      display->print(text.c_str());
      display->display();
    } else {
      ESP_LOGI(TAG, "display is failed");
    }
  }
}
};  // namespace curtains_controller
};  // namespace esphome