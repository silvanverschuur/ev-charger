#include "config.h"
#include "globals.h"
#include "wifi.h"
#include "ota.h"
#include "mqtt.h"

int pilotValue = 0;
int PWMDutyCycle = 0;
int chargingCurrent = DEFAULT_CURRENT;
ChargingState pilotState = STATE_F; // setting default to error, it will change as no power is detected at startup

void setChargingCurrent(int value) {
  chargingCurrent = value;

  // Charging current changed, force MQTT publish.
  publishMqtt(true);
}

void setChargingState(ChargingState state) {
  if (state == STATE_CUSTOM_ON && pilotState != STATE_C) {
    // Only update state to 'ON' if car is not charging
    pilotState = STATE_CUSTOM_ON;
    digitalWrite(RELAY_PIN, LOW); // Relay off
    // turn pwm to constant on, +12v on pilot so we do not get EVSE ERROR code (STATE_F)
    PWMDutyCycle = 1023; // turn off pwm, constant on
    ledcWrite(PWM_CHANNEL, PWMDutyCycle);
  } else {
    pilotState = state;
  }

  // State changed, force MQTT publish.
  publishMqtt(true);
}

int getChargingCurrentSetPoint() {
  return chargingCurrent;
}

float getControlPilotVoltage() {
  return (3.3 / 4096) * pilotValue;
}

int getPWM() {
  return pilotValue;
}

String getChargingState() {
  String result;

  switch (pilotState) {
    case STATE_A:
      result = "Not connected";
      break;
    case STATE_B:
      result = "Connected";
      break;
    case STATE_C:
      result = "Charging";
      break;
    case STATE_D:
      result = "Ventilation required";
      break;
    case STATE_E:
      result = "No power";
      break;
    case STATE_F:
      result = "Error";
      break;
    case STATE_CUSTOM_OFF:
      result = "Off";
      break;
    case STATE_CUSTOM_ON:
      result = "On";
      break;
    default:
      result = "Unknown";
  }

  return result;
}

int chargingPWM(int ampsToConvert) {
  float pwmsignal = ampsToConvert / 0.05859375; // 0.05859375 is 1/1024 of 1A when using 10bit resolution
  return (round(pwmsignal) - 1);
}


int checkAnalog(int analogPinToTest, int noSamples) {
  // the op-amp outputs a square wave for the most part so we find the peak in 'noSamples' tries ;)
  int maximum = 0;
  for (int i = 0; i <= noSamples; i++) {
    int value = analogRead(analogPinToTest);
    if (value >= maximum) {
      maximum = value;
    }
  }

  return maximum;
}

ChargingState getStateForADCValue(int adc_value) {
  if (adc_value >= 3779 && adc_value < 4096) {
    return STATE_A;
  }
  else if (adc_value >= 3150 && adc_value < 3779) {
    return STATE_B;
  }
  else if (adc_value >= 2618  && adc_value < 3150) {
    return STATE_C;
  }
  else if (adc_value >= 2166  && adc_value < 2618) {
    return STATE_D;
  }
  else if (adc_value >= 1700  && adc_value < 2166) {
    return STATE_E;
  }
  else if (adc_value >= 0  && adc_value < 1700) {
    return STATE_F;
  }

  return STATE_F;
}

bool CheckState(ChargingState oldChargingState, int adc_value) {
  ChargingState newState = getStateForADCValue(adc_value);
  int ampsPWM = chargingPWM(chargingCurrent);

  bool update = oldChargingState != newState || PWMDutyCycle != ampsPWM;

  if (update) {
    pilotState = newState;

    switch (pilotState) {
      case STATE_A:
        digitalWrite(RELAY_PIN, LOW);
        PWMDutyCycle = 1023; // turn off pwm, constant on
        ledcWrite(PWM_CHANNEL, PWMDutyCycle);
        delay(10); //Ed
        break;
      case STATE_B:
        digitalWrite(RELAY_PIN, LOW);
        // Advertize 1kHz square wave and wait until EV goes to charging mode
        PWMDutyCycle = ampsPWM; // % of 1023 max = Square wave
        ledcWrite(PWM_CHANNEL, PWMDutyCycle);
        Serial.println(ampsPWM);
        delay(20);
        break;
      case STATE_C:
        // Advertize charging capacity
        PWMDutyCycle = ampsPWM; // % of 1023 max =14A
        ledcWrite(PWM_CHANNEL, PWMDutyCycle);
        delay(10); // simulate relay closing time
        digitalWrite(RELAY_PIN, HIGH);
        delay(10); //Ed
        break;
      case STATE_D:
        digitalWrite(RELAY_PIN, LOW); // no charging
        // Advertize charging capacity
        PWMDutyCycle = ampsPWM; // turn off pwm
        ledcWrite(PWM_CHANNEL, PWMDutyCycle);
        delay(10); //Ed
        break;
      case STATE_E:
        digitalWrite(RELAY_PIN, LOW); // no charging
        PWMDutyCycle = 1023; // // set +12V DC on pilot
        ledcWrite(PWM_CHANNEL, PWMDutyCycle);
        delay(10); //Ed
        break;
      case STATE_F:
        digitalWrite(RELAY_PIN, LOW); // no charging
        // Advertize charging capacity
        PWMDutyCycle = 0; // turn off pwm
        ledcWrite(PWM_CHANNEL, PWMDutyCycle);
        break;
    }
  }

  return update;
}

void setup() {
  Serial.begin(115200);

  configureWifi();
  configureOTA();
  configureMqttClient();
  configureInputOuputs();
}

void configureInputOuputs() {
  pinMode(PILOT_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Initialize PWM pin
  ledcAttachPin(PILOT_GPIO, PWM_CHANNEL);
  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
}

void loop() {
  ArduinoOTA.handle();

  if (connectMqtt()) {
    mqttClient.loop();
  }

  bool forcePublish = false;

  // Reading pilot voltage value, analog is on square wave, we must get the maximum (and minimum)
  pilotValue = checkAnalog(PILOT_PIN, 400);

  // if charger is set to off then we will not change states
  if (pilotState == STATE_CUSTOM_OFF) {
    digitalWrite(RELAY_PIN, LOW); // no charging
    // Advertize charging capacity
    PWMDutyCycle = 0; // turn off pwm
    ledcWrite(PWM_CHANNEL, PWMDutyCycle);
  } else {
    forcePublish = CheckState(pilotState, pilotValue);
  }

  publishMqtt(forcePublish);
}
