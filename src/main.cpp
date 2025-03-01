#include <Arduino.h>
#include "M5AtomS3.h"
#include <M5GFX.h>
#include "Unit_Encoder.h"

#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

const int encoderAPin = 38;     // GPIO 38: Encoder A (SCM010 Pin 5 Brown)
const int encoderBPin = 39;     // GPIO 39: Encoder B (SCM010 Pin 6 Yellow)

volatile long encoderCount = 0;
unsigned long lastTime = 0;
int rpm = 0;

const int speedPin = 6;        // GPIO 6: Speed regulation  (SCM010 Pin 1 White)
const int startStopPin = 5;    // GPIO 5: Start/Stop & Torque limitation (SCM010 Pin 2 Grey)
const int directionPin = 7;    // GPIO 7: Rotation direction (SCM010 Pin 3 Purple)
const int speedModePin = 8;    // GPIO 8: Speed High / Low (SCM010 Pin 7 Green)

int relative_change = 0; // Relative change in encoder value

int pwmValue = 0; // Default PWM value
//int pwmValue = 3000; // Default target RPM
int direction = 1; // Default direction (true for CW)
bool motorRunning = false;

// Define max RPM for your motor
const int maxRPM = 5000;

// PWM configuration for ESP32
const int pwmChannel = 0;
const int pwmFrequency = 5000;
const int pwmResolution = 8;

const char* ssid = "SimplexMotion_AP";
const char* password = "12345678";



WebServer server(80);

int ontime;
//int direction;
//uint64_t direction = 1000; //delay between runs
int save_direction = 10; // direction saved in EEPROM as int (direction divided by 100)
uint64_t tempus;
bool newpress = true; // monitor if button just pressed 
int mstatus = 0; // defines which state the system is in

signed short int last_value = 0;
signed short int last_btn = 1;

M5GFX display;
M5Canvas canvas(&display);
Unit_Encoder sensor;
//M5UnitHbridge driver;

void handleRoot() {
    String html = "<html><body style=\"font-size: 18px;\">";
    html += "<meta name=\"viewport\" content=\"width=390, initial-scale=1\"/>";
    html += "<h1 style=\"font-size: 24px;\">Motor Control Settings</h1>";
    html += "<form action=\"/update\" method=\"POST\">";
    html += "pwmValue: <input type=\"text\" name=\"pwmValue\" value=\"" + String(pwmValue) + "\" style=\"font-size: 18px;\"><br>";
    html += "Direction: <input type=\"text\" name=\"direction\" value=\"" + String(direction) + "\" style=\"font-size: 18px;\"><br>";
    html += "<input type=\"submit\" value=\"Save\" style=\"font-size: 18px;\">";
    html += "</form>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleUpdate() {
    if (server.hasArg("pwmValue") && server.hasArg("direction")) {
        //ontime = server.arg("ontime").toInt();
        pwmValue = server.arg("pwmValue").toInt();
        AtomS3.Display.clear();
        AtomS3.Display.drawString(String(pwmValue), 10, 30);
        ledcWrite(pwmChannel, pwmValue); // Set the motor speed
        //int pwmValue = map(pwmValue, 0, maxRPM, 0, pow(2, pwmResolution) -1);
        direction = server.arg("direction").toInt();
        AtomS3.Display.drawString(String(direction), 10, 60);
        digitalWrite(directionPin, direction ? HIGH : LOW); // Set the direction

        //EEPROM.put(0, pwmValue);
        //EEPROM.put(sizeof(pwmValue), direction);
        //EEPROM.commit();

        server.send(200, "text/html", "<html><body><h1>Settings Saved</h1><a href=\"/\">Go Back</a></body></html>");
    } else {
        server.send(400, "text/html", "<html><body><h1>Invalid Input</h1><a href=\"/\">Go Back</a></body></html>");
    }
}

void updateEncoder() {
    if (digitalRead(encoderBPin) == HIGH) {
      encoderCount++;
    } else {
      encoderCount--;
    }
  }

void setup() {

    pinMode(encoderAPin, INPUT_PULLUP);
    pinMode(encoderBPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(encoderAPin), updateEncoder, RISING);

    pinMode(speedPin, OUTPUT);
    pinMode(startStopPin, OUTPUT);
    pinMode(directionPin, OUTPUT);
    pinMode(speedModePin, OUTPUT);

    // Setup PWM for speed control
    ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
    ledcAttachPin(speedPin, pwmChannel);

    Serial.begin(115200);
    EEPROM.begin(512);
    EEPROM.get(0, pwmValue);
    EEPROM.get(sizeof(pwmValue), direction);
    WiFi.softAP(ssid, password);
    Serial.println("Access Point Started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/update", HTTP_POST, handleUpdate);
    server.begin();
    Serial.println("HTTP server started");

    Wire.begin(2, 1);
    auto cfg = M5.config();
    AtomS3.begin(cfg);
    sensor.begin();

    AtomS3.Display.setTextColor(WHITE);
    AtomS3.Display.setTextSize(3);
    AtomS3.Display.clear();
    tempus = millis();
    int pwmValue = map(pwmValue, 0, maxRPM, 0, pow(2, pwmResolution) -1);
}

void loop() {
    server.handleClient();

    bool btn_status = sensor.getButtonStatus();
    if (last_btn != btn_status) {
        if (!btn_status) {
            mstatus = mstatus + 1;
            if (mstatus == 4) mstatus = 0;
            AtomS3.Display.clear();
            newpress = true;
        }
        last_btn = btn_status;
    }

    // Calculate RPM every second
    if (millis() - lastTime >= 1000) {
        AtomS3.Display.setTextColor(BLACK);
        AtomS3.Display.drawString(String(rpm), 10, 90); // Clear the previous value
        rpm = (encoderCount * 60) / 1024; // Assuming encoder PPR
        encoderCount = 0;
        lastTime = millis();
        AtomS3.Display.setTextColor(WHITE);
        AtomS3.Display.drawString(String(rpm), 10, 90);
        }

switch (mstatus) {

        case 0: //-------------- RUNNING MODE -----------------//
        { 
            if (newpress) {
                digitalWrite(directionPin, direction ? HIGH : LOW); // Set the direction
                ledcWrite(pwmChannel, pwmValue); // Set the motor speed
                AtomS3.Display.drawString("Running", 5, 0);
                AtomS3.Display.drawString(String(pwmValue), 10, 30);
                AtomS3.Display.drawString(String(direction), 10, 60);
                newpress = false;
            }

                AtomS3.update();
                if (AtomS3.BtnA.wasPressed()) { // Toggle the motor state
                    motorRunning = !motorRunning;
                    digitalWrite(startStopPin, motorRunning ? HIGH : LOW);
                    AtomS3.Display.setTextColor(BLACK);
                    AtomS3.Display.drawString(motorRunning ? "Stopped" : "Running", 5, 0);
                    AtomS3.Display.setTextColor(WHITE);
                    AtomS3.Display.drawString(motorRunning ? "Running" : "Stopped", 5, 0);
                }

            break;
        } // end of case 0


        case 1: // read encoder for ON time in ms
        {
            signed short int encoder_value = sensor.getEncoderValue();

            if (newpress) {
                AtomS3.Display.drawString("Speed", 5, 0);
                AtomS3.Display.drawString(String(pwmValue), 10, 30);
                last_value = encoder_value; // Update the last value
                newpress = false;
            }

            if (last_value != encoder_value) {
                if (encoder_value - last_value > 1) {
                    relative_change = 1;
                    last_value = encoder_value;
                }
                else if (encoder_value - last_value < -1) {
                    relative_change = -1;
                    last_value = encoder_value;
                }
                else {
                    relative_change = 0;
                }

                AtomS3.Display.setTextColor(BLACK);
                AtomS3.Display.drawString(String(pwmValue), 10, 30); // Clear the previous value
                AtomS3.Display.setTextColor(WHITE);
                pwmValue = pwmValue + relative_change;
                ledcWrite(pwmChannel, pwmValue); // Set the PWM value
                AtomS3.Display.drawString(String(pwmValue), 10, 30); // Display the updated change
            }
            delay(20);
            break;
        } // end of case 1

        case 2: // read encoder for direction in ms
        {
            signed short int encoder_value = sensor.getEncoderValue();

            if (newpress) {
                AtomS3.Display.drawString("DIR", 5, 0);
                AtomS3.Display.drawString(String(direction), 10, 60);
                last_value = encoder_value; // Update the last value
                newpress = false;
            }

            if (last_value != encoder_value) {
                AtomS3.Display.setTextColor(BLACK);
                AtomS3.Display.drawString(String(direction), 10, 60);
                AtomS3.Display.setTextColor(WHITE);
                
                if (last_value < encoder_value) {
                    direction = 1;
                }
                if (last_value > encoder_value) {   
                    direction = 0;
                }
                
                AtomS3.Display.drawString(String(direction), 10, 60);
                last_value = encoder_value;
            }
            break;    
        } // end of case 2

        case 3: // check if we want to save ontime and direction by pres Atom button
        {
            if (newpress) {
                AtomS3.Display.drawString("Save ?", 5, 0);
                AtomS3.Display.drawString(String(direction), 10, 60);
                AtomS3.Display.drawString(String(pwmValue), 10, 30);
                newpress = false;
            }
            
            AtomS3.update();
            if (AtomS3.BtnA.wasPressed()) { // Save to EEPROM
                EEPROM.put(0, pwmValue);
                EEPROM.put(sizeof(pwmValue), direction);
                EEPROM.commit();

                // Flash the display in black and green
                for (int i = 0; i < 3; i++) {
                    AtomS3.Display.fillScreen(TFT_BLACK);
                    delay(200);
                    AtomS3.Display.fillScreen(TFT_GREEN);
                    delay(200);
                }

                // Display "Saved" with a black background
                AtomS3.Display.fillScreen(TFT_BLACK);
                AtomS3.Display.setTextColor(TFT_WHITE, TFT_BLACK); // White text on black background
                AtomS3.Display.drawString(String(pwmValue), 10, 30);
                AtomS3.Display.drawString(String(direction), 10, 60);
                AtomS3.Display.drawString("Saved", 10, 0);

            }
            break;    
        } // end of case 3

    } // end of switch cases
}