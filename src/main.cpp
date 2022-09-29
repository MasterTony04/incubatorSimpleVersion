#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <Adafruit_Sensor.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <SimpleTimer.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "DHT.h"

#define DHTPIN A0     // what pin we're connected to
#define DHTPIN2 A1     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define ONE_WIRE_BUS A1

DHT dht(DHTPIN, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

//float celcius = 0;

LiquidCrystal_I2C lcd(0x27, 20, 4);

// Classes
#include "Incubation_Screen.h"
#include "fanControl.h"


//Timers
SimpleTimer firstTimer(1000);
SimpleTimer ignitionTimer(15000);  //5 secs initially
SimpleTimer motorTimer(5000);

//Variables
//TODO: pin A2 is unused
uint8_t humidifier = 9, igniter = 10, heater = 11, gasValve = 12;
uint8_t buzzer = A3;
uint8_t upButton = 2, downButton = 4, selectButton = 3, okButton = 0;
uint8_t powerIn = 7;


uint8_t state1, state2, state3;
uint8_t idle = 1;
uint8_t startStatus = 1;
bool processInitiated = false;
float minTempValue = 37.9, maxTempValue = 38.2, minHumidityIncubation = 50.0, maxHumidityIncubation = 55, minHatchHum = 70, maxHatchHum = 75;


////led indicator

bool led_Alarm = false, led_Fan = false, led_Heat = false, led_Hum = false, led_cool = false, led_gas = false;

float hum, hum2;  //Stores humidity value
float temp, temp2; //Stores temperature value

// transmission variables
SoftwareSerial WiFiSerial(5, 6); //Change pins here  tx=5, rx=6 on pcb

unsigned long millisTime, secondsHolder = 59;

int seconds, minutes = 59, hours = 1, daysCounter = 0, day = 1, incubationDays = 18, hatchingDays = 3;
int hoursCounter = 0, minutesCounter = 0;
bool startMotor = false;

//Buzzer
bool buzzerToneActive = false;

//PowerIn
bool powerInStatus = false,startFanOut = false, powerPriority=false,ignitionStarted=false,ignitionDone=false;

void setup() {
// write your initialization code here

    WiFiSerial.begin(9600);
    Serial.begin(9600);


    processInitiated = EEPROM.read(2);
    daysCounter = EEPROM.read(5);
   // EEPROM.write(5,1);
   // daysCounter = 1;
    day = EEPROM.read(6);
   // EEPROM.write(6,18);
   // day =18;
    incubationDays = EEPROM.read(7);
    hatchingDays = EEPROM.read(8);

    //sensors initialization
    dht.begin();
    dht2.begin();

    lcd.begin();
    lcd.clear();
    lcd.backlight();

    //variables in classes
    FN.fanSetup(8);  //pin 8 fanIn
    IS.IncubationMenuSetup();

    pinMode(igniter, OUTPUT);
    pinMode(humidifier, OUTPUT);
    pinMode(upButton, INPUT);
    pinMode(selectButton, INPUT);
    pinMode(downButton, INPUT);
    pinMode(heater, OUTPUT);
    pinMode(gasValve, OUTPUT);
    pinMode(powerIn, INPUT);
    pinMode(buzzer, OUTPUT);
    lcd.clear();
}

void sendMasterData() {
    StaticJsonDocument<512> doc;

    doc["startFanOut"] = startFanOut;
    doc["startMotor"] = startMotor;
    doc["temp"] = temp;
    doc["hum"] = hum;
    doc["powerPriority"] = powerPriority;
    doc["incubationDays"] = incubationDays;
    doc["hatchingDays"] = hatchingDays;
    doc["day"] = day;
    doc["led_Fan"] = led_Fan;
    doc["led_Hum"] = led_Hum;
    doc["led_Heat"] = led_Heat;
    doc["led_gas"] = led_gas;
    doc["led_Alarm"] = led_Alarm;

    serializeJson(doc, WiFiSerial);
    // serializeJson(doc, Serial);
    String sendStatus = "";
    sendStatus = WiFiSerial.readStringUntil('0');
}

void loop() {

// write your code here
    //DHT 22 temp & Hum Sensor
    hum = dht.readHumidity();
    hum2 = dht2.readHumidity();
    temp = dht.readTemperature();
    temp2 = dht2.readTemperature();

    sendMasterData();

    millisTime = (millis() / 1000);
    seconds = secondsHolder - millisTime;

    if (seconds < 0) {
        seconds = 59;
        minutes--;
        secondsHolder = millisTime + 59;

    }

    if (minutes < 0) {
        hours--;
        minutes = 59;
    }

    if (hours < 0) {
        hours = 1;
        daysCounter++;
        EEPROM.write(5, daysCounter);

        //motorPin
        if (day < incubationDays) {
            startMotor = true;
            motorTimer.reset();
        }

        if (daysCounter > 11) {

            //days counter (0 - 11)
            daysCounter = 0;
            EEPROM.write(5, daysCounter);
            day++;
            if (day > (incubationDays + hatchingDays + 2)) {
                //resetIncubation();
                //TODO:exit();
            }

            //TODO: check if this is not performed on the resetIncubation() method
            EEPROM.write(6, day);
        }
    }

    //Egg turning motor initiator
    if (motorTimer.isReady() && startMotor == true) {
        startMotor = false;
        motorTimer.reset();
    }

    state1 = digitalRead(upButton);
    state2 = digitalRead(downButton);
    state3 = digitalRead(selectButton);

    if(state3){
        okButton++;
        if(okButton==1 &&processInitiated==false){
            lcd.clear();
            processInitiated = true;
            EEPROM.write(2, processInitiated);

            firstTimer.reset();
            secondsHolder = millisTime + 59;
            seconds = secondsHolder - millisTime;
            minutes = 59;
            hours = 1;

            daysCounter = 0;
            EEPROM.write(5, daysCounter);

            day = 0;
            EEPROM.write(6, day);
        }
    }

    if(state1){
        lcd.clear();
        okButton=0;

        processInitiated = false;
        EEPROM.write(2, processInitiated);

        daysCounter = 0;
        EEPROM.write(5, daysCounter);

        day = 0;
        EEPROM.write(6, day);

    }


    if (processInitiated) {

        IS.IncubationScreen(temp, hum, temp2, hum2, powerPriority, day, temp2, hours, minutes, seconds);
        FN.startFan();
        led_Fan =HIGH;


        //// //temp Check
        if (temp <= minTempValue) {
//                if (buzzerToneActive == true) {
//
//                    digitalWrite(buzzer, HIGH);          // changed here
//                } else {
//                    digitalWrite(buzzer, LOW);
//
//                }
//
//
            if (powerPriority == 0) {
//  // switch on the heaters because there is power
                digitalWrite(heater, HIGH);
                led_Heat=true;
                ignitionStarted = false;
                digitalWrite(gasValve, LOW);
                digitalWrite(igniter, LOW);

            }
//
//
            if (powerPriority == 1) {       //TODO: switch on gas
                if (ignitionStarted == false) {
                    ignitionStarted = true;
                    ignitionDone = true;
                    digitalWrite(gasValve, HIGH);
                    digitalWrite(igniter, HIGH);
//                        sparkCounter = 0;
//                        // =====================
                    digitalWrite(heater, LOW);
                    led_Heat=false;
                    led_gas= true;
                    ignitionTimer.reset();
                }
            }
        }
//            else {
//                noTone(buzzer);
//
//                //===========================
        // }

        if (temp >= 38.2 && powerPriority == 0) {
            //TODO:=======================
            digitalWrite(heater, LOW);
            led_Heat = LOW;
        }
        if (temp >= maxTempValue) {
            //TODO: find what ignitionStarted meant
            ignitionStarted = false;
            digitalWrite(gasValve, LOW);
            digitalWrite(igniter, LOW);
            digitalWrite(heater, LOW);
            led_Heat = LOW;

        }

////======================================

//Buzzer Timer counter
        if (firstTimer.isReady()) {

            if (temp < 37.2) {
                buzzerToneActive = !buzzerToneActive;
            } else {
                buzzerToneActive = false;
            }

            firstTimer.reset();
        }

        //TODO: igniter timer goes here

        if (ignitionTimer.isReady() && ignitionDone == true) {
            digitalWrite(igniter, LOW);
            ignitionDone = false;
            ignitionTimer.reset();
        }


        if (temp > 38.8) {

            startFanOut = true;
        }
        else {

            startFanOut = false;
        }

        //humidity check
//        if (day < incubationDays) {
//
//            if (hum < minHumidityIncubation) {
//
//                digitalWrite(humidifier, HIGH);
//            }
//
//            if (hum >= maxHumidityIncubation) {
//                digitalWrite(humidifier, LOW);
//            }
//
//        }
//        else{

            if (hum < minHumidityIncubation) {
                digitalWrite(humidifier, HIGH);
            }

            if (hum >= maxHumidityIncubation) {
                digitalWrite(humidifier, LOW);
            }
      //  }


    }
    else {
        lcd.setCursor(0, 1);
        lcd.println("AFRITECH INCUBATOR ");
        //TODO: MK03 lcd.setCursor()
        FN.stopFan();
        led_Fan= LOW;
        digitalWrite(heater, LOW);
        led_Heat = LOW;
        digitalWrite(gasValve, LOW);
        digitalWrite(igniter, LOW);
    }

    //power
    powerInStatus = digitalRead(powerIn);


    if (powerInStatus) {
        powerPriority = 0;        //electricity
    } else {
        powerPriority = 1;          // gas
    }
}