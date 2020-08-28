#include <app.h>
#include <models/light.h>

Amp* App::amp = nullptr;

App::App(Amp *instance) {
  amp = instance;
  amp->config.addConfigListener(this);

  configUpdatedQueue = xQueueCreate(1, sizeof(bool));
  lightModeQueue = xQueueCreate(1, sizeof(LightMode));
  vehicleQueue = xQueueCreate(5, sizeof(VehicleState));
}

void App::onPowerUp() { 
#ifdef BLE_ENABLED
  BluetoothLE::bleReady.wait();

  // workaround for weird bug where the first initialized service is duplicated / empty
  auto dummyService = amp->ble.server->createService(NimBLEUUID((uint16_t)0x183B));
  dummyService->start();
  
  // startup bluetooth services
  deviceInfoService = new DeviceInfoService(amp->ble.server);
  batteryService = new BatteryService(amp->ble.server);
  vehicleService = new VehicleService(&(amp->motion), amp->power, amp->ble.server, this);
  configService = new ConfigService(&(amp->config), amp->ble.server);
  updateService = new UpdateService(amp->updater, amp->ble.server);

  // listen to power updates
  amp->power->addPowerLevelListener(batteryService);

  // listen to render host changes for the vehicle service
  addRenderListener(vehicleService);

  // startup advertising
  amp->ble.startAdvertising();

  // notify lights changed
  if (renderer != nullptr)
    notifyLightsChanged(renderer->getBrakeCommand(), renderer->getTurnLightCommand(), renderer->getHeadlightCommand());
#endif

  // listen to motion changes
  amp->motion.addMotionListener(this);
}

void App::onPowerDown() {
  Log::trace("App power down");

  if (renderHostHandle != NULL) {
    vTaskDelete(renderHostHandle);
    renderHostHandle = NULL;
  }

  if (renderer != NULL)
    renderer->shutdown();
}

void App::onConfigUpdated() {
  config = &Config::ampConfig;

  // start new render host
  Log::trace("Renderer starting after config update");
  setLightMode(config->prefs.renderer);
}

void App::process() {
  bool valid;

  if (uxQueueMessagesWaiting(configUpdatedQueue)) {
    if (xQueueReceive(configUpdatedQueue, &valid, 0) && valid)
      onConfigUpdated();
  }

  if (uxQueueMessagesWaiting(lightModeQueue)) {
    LightMode mode;
    if (xQueueReceive(lightModeQueue, &mode, 0))
      setLightMode(mode);
  }

  VehicleState state;
  bool newVehicleState = false;
  while (uxQueueMessagesWaiting(vehicleQueue)) {
    newVehicleState = xQueueReceive(vehicleQueue, &state, 0);
  }

  if (newVehicleState) {
    if (vehicleState.acceleration != state.acceleration)
      onAccelerationStateChanged(state.acceleration);
    
    if (vehicleState.turn != state.turn)
      onTurnStateChanged(state.turn);

    if (vehicleState.orientation != state.orientation)
      onOrientationChanged(state.orientation);

  #ifdef BLE_ENABLED
    if (vehicleService != nullptr)
      vehicleService->onVehicleStateChanged(state);
  #endif
    
    vehicleState = state;
  }

#ifdef BLE_ENABLED
  vehicleService->process();
  batteryService->process();
  updateService->process();
#endif

  // if (renderer != NULL)
  //  renderer->process();
}

void App::onAccelerationStateChanged(AccelerationState state) {
  // Log::trace("on acceleration state changed");
  if (renderer != NULL) {
    LightCommand command;

    switch (state) {
      case AccelerationState::Braking:
        command = LightCommand::LightsBrake;
        break;
      default:
      case AccelerationState::Neutral:
        command = LightCommand::LightsRunning;
        break;
    }

    setBrakes(command);
  }
}

void App::onTurnStateChanged(TurnState state) {
  if (renderer != NULL) {
    LightCommand command;

    switch (state) {
      case TurnState::Left:
        command = LightCommand::LightsTurnLeft;
        break;
      case TurnState::Right:
        command = LightCommand::LightsTurnRight;
        break;
      case TurnState::Hazard:
        command = LightCommand::LightsTurnHazard;
        break;
      case TurnState::Center:
      default:
        command = LightCommand::LightsTurnCenter;
        break;
    }

    setTurnLights(command);
  }
}

void App::onOrientationChanged(Orientation state) {
  if (renderer != NULL) {
    LightCommand command;

    switch (state) {
      case Orientation::TopSideUp:
        command = LightCommand::LightsOff;
        break;
      default:
        command = LightCommand::LightsReset;
        break;
    }

    setTurnLights(command);
    setBrakes(command);
    setHeadlight(command);
  }
}

void App::setLightMode(LightMode mode) {
  if (lightMode != mode) {
    if (renderHostHandle != NULL) {
      vTaskDelete(renderHostHandle);
      renderer->shutdown();

      renderer = NULL;
      renderHostHandle = NULL;
    }

    switch (mode) {
      case LightMode::TheaterChaseRainbowMode:
        renderer = new PatternRenderer(amp->lights, "theater-chase-rainbow");
        break;
      case LightMode::TheaterChaseMode:
        renderer = new PatternRenderer(amp->lights, "theater-chase");
        break;
      case LightMode::RainbowMode:
        renderer = new PatternRenderer(amp->lights, "rainbow");
        break;
      case LightMode::LightningMode:
        renderer = new PatternRenderer(amp->lights, "lightning");
        break;
      case LightMode::RunningMode:
      default:
        renderer = new RunningRenderer(amp->lights, config);
        amp->lights->render();
      break;
    }

    xTaskCreatePinnedToCore(startRenderHost, "renderer", 8 * 1024, this, 5, renderHostHandle, 1);
    lightMode = mode;

    notifyLightsChanged();
  }
}

void App::startRenderHost(void *params) {
  App *app = (App*)params;
  Amp *amp = app->amp;

  amp->motion.resetMotionDetection();
  
  for (;;) {
    if (app->renderer != NULL)
      app->renderer->process();

    delay(50);
  }

  app->renderHostHandle = NULL;
  vTaskDelete(NULL);
}

void App::setHeadlight(LightCommand command) {
  if (renderer != NULL && renderer->headlightQueue != NULL)
    xQueueSend(renderer->headlightQueue, &command, 0);

  // update listeners
  notifyLightsChanged(NoCommand, NoCommand, command);
}

void App::setBrakes(LightCommand command) {
  if (renderer != NULL && renderer->brakelightQueue != NULL)
    xQueueSend(renderer->brakelightQueue, &command, 0);

  // update listeners
  notifyLightsChanged(command, NoCommand, NoCommand);
}

void App::setTurnLights(LightCommand command) {
  if (renderer != NULL && renderer->turnlightQueue != NULL)
    xQueueSend(renderer->turnlightQueue, &command, 0);

  // update listeners
  notifyLightsChanged(NoCommand, command, NoCommand);
}

void App::notifyLightsChanged(LightCommand brakeCommand, LightCommand turnCommand, LightCommand headlightCommand) {
  LightCommands commands;
  commands.mode = lightMode;
  commands.brakeCommand = brakeCommand;
  commands.turnCommand = turnCommand;
  commands.headlightCommand = headlightCommand;

  for (auto listener : renderListeners) {
    if (listener->lightsChangedQueue != NULL)
      xQueueSend(listener->lightsChangedQueue, &commands, 0);
  }
}