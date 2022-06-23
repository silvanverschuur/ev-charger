#include <PubSubClient.h>

const int VALUE_LENGTH = 16;

int mqttPublishInterval = MQTT_DEFAULT_PUBLISH_INTERVAL; // Default publish interval
long lastMqttConnectionAttempt = 0;
long lastMqttPublish = 0;
float lastVoltage = -1;
String lastState = "";
int lastPWM = -1;
int lastSetPoint = -1;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void disconnectMqtt() {
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
}

boolean connectMqtt() {
  if (mqttClient.connected()) {
    return true;
  }

  long now = millis();
  if (now - lastMqttConnectionAttempt < MQTT_RECONNECT_DELAY) {
    return false;
  }

  lastMqttConnectionAttempt = now;

  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("Connected to MQTT server");
    mqttClient.publish(MQTT_TOPIC_ONLINE, "true");
    mqttClient.subscribe(MQTT_TOPIC_SET_ON);
    mqttClient.subscribe(MQTT_TOPIC_SET_SETPOINT);
  } else {
    Serial.print("Cannot connect to MQTT server, result ");
    Serial.print(mqttClient.state());
    Serial.println();
    delay(50);
  }

  return mqttClient.connected();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char buff[length];
  for (int i = 0; i < length; i++)
  {
    buff[i] = (char)payload[i];
  }
  buff[length] = '\0';

  String message = String(buff);

  if (strcmp(topic, MQTT_TOPIC_SET_ON) == 0) {
    // valid values: true/false. Use c_str() to convert String to char[] to make sure we can compare strings in ASCII format.
    bool on = strcmp(message.c_str(), "true") == 0;
    setChargingState(on ? STATE_CUSTOM_ON : STATE_CUSTOM_OFF);
  } else if (strcmp(topic, MQTT_TOPIC_SET_SETPOINT) == 0) {
    int current = message.toInt();

    if (current >= MIN_CURRENT && current <= MAX_CURRENT) {
      Serial.print("Update current setpoint to:");
      Serial.println(current);
      setChargingCurrent(current);
    } else {
      Serial.print("Current setpoint must be between ");
      Serial.print(MIN_CURRENT);
      Serial.print(" and ");
      Serial.print(MAX_CURRENT);
      Serial.print(", your value: ");
      Serial.println(current);
    }
  }
}

void configureMqttClient() {
  mqttClient.setServer(MQTT_SERVER, 1883);
  mqttClient.setCallback(mqttCallback);
}

void publishString(const char *topic, String value) {
  char buffer[50];
  value.toCharArray(buffer, 50);
  mqttClient.publish(topic, buffer);
}

void publishStringIfModified(const char *topic, String oldValue, String newValue) {
  if (strcmp(oldValue.c_str(), newValue.c_str()) != 0) {
    publishString(topic, newValue);
  }
}

void publishFloat(const char *topic, float value, int precision) {
  char buffer[VALUE_LENGTH];
  dtostrf(value, 1, precision, buffer);
  mqttClient.publish(topic, buffer);
}

void publishFloatIfModified(const char *topic, float oldValue, float newValue, int precision) {
  if (fabs(newValue - oldValue) < 0.03f) {
    return;
  }

  publishFloat(topic, newValue, precision);
}

void publishInt(const char *topic, int value) {
  char buffer[VALUE_LENGTH];
  mqttClient.publish(topic, itoa(value, buffer, 10));
}

void publishIntIfModified(const char *topic, int oldValue, int newValue) {
  if (oldValue != newValue) {
    publishInt(topic, newValue);
  }
}

void publishMqtt(bool forcePublish) {
  long now = millis();

  if (!forcePublish && (now - lastMqttPublish < mqttPublishInterval)) {
    return;
  }

  lastMqttPublish = now;

  String state = getChargingState();
  float voltage = getControlPilotVoltage();
  int pwm = getPWM();
  int setPoint = getChargingCurrentSetPoint();

  publishStringIfModified(MQTT_TOPIC_STATE, lastState, state);
  publishFloatIfModified(MQTT_TOPIC_CP_VOLTAGE, lastVoltage, voltage, 2);
  publishIntIfModified(MQTT_TOPIC_PWM, lastPWM, pwm);
  publishIntIfModified(MQTT_TOPIC_SETPOINT, lastSetPoint, setPoint);

  lastState = state;
  lastVoltage = voltage;
  lastPWM = pwm;
  lastSetPoint = setPoint;
}
