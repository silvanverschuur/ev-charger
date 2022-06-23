# EV-charger based on ESP32

This software is based on the following code and is extended with MQTT support: 
https://forum.arduino.cc/t/evse-charger-miev-leaf-tesla-6-16a-esp32/878422


### Required libraries
* PubSubClient (MQTT)

### Building the software
Copy the `config.h.example` file to `config.h` and change settings (WiFi / MQTT / pins / etc).

Open `ev-charger.ino` with Arduino IDE and compile/upload to ESP32 module.

### ESP32-WROOM-32 Pinout
<pre>
 3v3 to opamp _____________________
       3.3V--|*                   *| -- GND
 Jtag Reset--|*                   *| -- GPIO_23  
    GPIO_36--|*                   *| -- GPIO_22
    GPIO_39--|*                   *| -- GPIO__1
    GPIO_34--|*Analog IN          *| -- GPIO__3
    GPIO_35--|*Current sens       *| -- GPIO_21
    GPIO_32--|*PWM 1KhZ           *| -- NC / GND
    GPIO_33--|*230V Relay         *| -- GPIO_19
    GPIO_25--|*                   *| -- GPIO_18
    GPIO_26--|*                   *| -- GPIO__5
    GPIO_27--|*.                  *| -- GPIO_17
    GPIO_14--|*                   *| -- GPIO_16
    GPIO_12--|*                   *| -- GPIO__4
       GND---|*                   *| -- GPIO__0
    GPIO_13--|*                   *| -- GPIO__2
    GPIO__9--|*        Temp Sensor*| -- GPIO_15 <---> Dallas DS18b20 Temperature sensor
    GPIO_10--|*                   *| -- GPIO__8
    GPIO_11--|*                   *| -- GPIO__7
       5.0V--|*   EN  USB  BOOT   *| -- GPIO__6
             |____**__| |__**______|

</pre>

Do not use ADC2 pins to measure when using WiFi.
WiFi causes interrupts on those analog_in pins.
Use GPIO 32, 33, 34, 35, 36, 39 only for analog measurements.

Charging States:
|State|Pilot Voltage|EV Resistance|Description| Analog theoretic: (if pwm is on 1khz)|
|--|--|--|--|--|
|  State A|       12V |           N/A|         Not Connected|           3.177 V   = 3943 of 4096 on ADC|
|  State B|        9V |          2.7K|         Connected|               2.811 V   = 3489 of 4096 on ADC|
|  State C|        6V |        882 Ohm|       Charging|                2.445 V   = 3034 of 4096 on ADC|
| State D|        3V|          246 Ohm|       Ventilation Required|    2.079 V   = 2580 of 4096 on ADC|
 | State E|        0V|            N/A|         No Power|                1.713 V   = 2126 of 4096 on ADC|
|  State F |     -12V |           N/A |        EVSE Error  |         249.198 mV   =  309 of 4096 on ADC|

To test pilot voltage: take resistor from EV resistance table and put between pilot-pin and ground the pilot voltage will drop to 9V with 2.7K resistor etc, then you can measure the ADC values.

Best to test with 100% pwm = DC 12V from Op-Amp (no square wave), then simple `analogRead()` on PilotPin input will give you the desired ADC value.
  Second choice is driving the different voltages from a powersupply through the PP pin and ground
  (bias voltage must be persent on circuit or the ESP pins can be damaged)
  
  measured Analog:
  State A: 3.19V and ADC: 3950
  State B: 2.75V and ADC: 3408
  State C: 2.30V and ADC: 2847
  State D: 1.92V and ADC: 2390 
  State E: 1.57V and ADC: 1941
  State F:
  
Calculation for PWM signal to charge @ x AMPS: (valid for up to 51A)
AMPS = Duty cycle X 0.6 (duty cycle is in %)
Duty Cycle = AMPS / 0.6
<pre>
example: 6A / 0.6 = 10% PWM
        16A / 0.6 = 26.666% PWM
       10% PWM * 0.6 = 6A
       20% PWM * 0.6 = 12A
51-80A, AMPS = (Duty Cycle - 64)2.5
</pre>
         
(similar to https://ev-olution.yolasite.com/DIY-Arduino-EVSE.php?fbclid=IwAR23UIyWYvearjFswAKCXX8PWCxq1a4q0NeUoIQT9HDftjr3ByCuptJ1D0w)
(but adapted to 3.3V instead of 5V) => R4=47K, R3=68K, and 5V voltage source is changed to 3.3V
EV-Ground and ESP32 ground (and psu) must be connected together
https://www.ebay.com/itm/323540774103?hash=item4b54886cd7:g:vd4AAOSwIr9bkKIv at 150mA is sufficient to generate +-12V rails

<pre>            
            Op-AMP circuit:

        
                     3.3V       Pilot to car (PP pin on EV charging plug)
                     |          | 
                     R47K       |--|<-| TVS diode-P6KE16A->GND
                     |          |
         GND---R68K--|--R200K---|
                                |                    
                                |                                  ____________________
                                Pilot -- R1(1KOhm) Output1 Pin_1  |*                  *| Pin_8 +VCC (+12V)_____________________
10K-10K voltage divider from 3.3V->inverting input(1.75V)  Pin_2  |*      LF353       *| Pin_7 Output2 (not used)              |
                                        From ESP32 GPIO_32 Pin_3  |*      OP-AMP      *| Pin_6 Inverting input2 (not used)     |
                                        -VCC ------- -12V  Pin_4  |*                  *| Pin_5 Non inverting input2 (not used) |
                                                |                 |____________________|                                       |
                                                |                                                                              |
                                GND---->|0.1uF|---|0.1uF|->OP_AMP_pin_8(+12V)__________________________________________________|

</pre>

