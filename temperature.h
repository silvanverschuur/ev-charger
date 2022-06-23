// GPIO where the DS18B20 is connected to
const int oneWireBus = 15;  
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
float temperatureC = 0.00;