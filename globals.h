enum ChargingState {
  STATE_A, // Pilot Voltage  12V, Not connected,        ADC: 3950
  STATE_B, // Pilot Voltage   9V, Connected,            ADC: 3408
  STATE_C, // Pilot Voltage   6V, Charging,             ADC: 2847
  STATE_D, // Pilot Voltage   3V, Ventilation required  ADC: 2390
  STATE_E, // Pilot Voltage   0V, No Power              ADC: 1941
  STATE_F, // Pilot Voltage -12V, EVSE Error            ADC: <1500
  STATE_CUSTOM_OFF, // custom state for Home automation, no charging until STATE_CUSTOM_ON or STATE_A
  STATE_CUSTOM_ON
};

void setChargingCurrent(int value);
void setChargingState(ChargingState state);
String getChargingState();
float getControlPilotVoltage();
int getPWM();
int getChargingCurrentSetPoint();
