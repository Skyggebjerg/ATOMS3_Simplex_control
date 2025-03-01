#include <Arduino.h>
#include "M5AtomS3.h"
#include <M5GFX.h>
#include "Unit_Encoder.h"
//#include "M5UnitHbridge.h"

#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

const int speedPin = 6;        // GPIO 6: Speed regulation  (SCM010 Pin 1 White)
const int startStopPin = 5;    // GPIO 5: Start/Stop & Torque limitation (SCM010 Pin 2 Grey)
const int directionPin = 7;    // GPIO 7: Rotation direction (SCM010 Pin 3 Purple)
const int speedModePin = 8;    // GPIO 8: Speed High / Low (SCM010 Pin 7 Green)

int relative_change = 0; // Relative change in encoder value

int pwmValue = 0; // Default PWM value
int targetRPM = 3000; // Default target RPM
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
    html += "TargetRPM: <input type=\"text\" name=\"targetRPM\" value=\"" + String(targetRPM) + "\" style=\"font-size: 18px;\"><br>";
    html += "Direction: <input type=\"text\" name=\"direction\" value=\"" + String(direction) + "\" style=\"font-size: 18px;\"><br>";
    html += "<input type=\"submit\" value=\"Save\" style=\"font-size: 18px;\">";
    html += "</form>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleUpdate() {
    if (server.hasArg("targetRPM") && server.hasArg("direction")) {
        //ontime = server.arg("ontime").toInt();
        targetRPM = server.arg("targetRPM").toInt();
        int pwmValue = map(targetRPM, 0, maxRPM, 0, pow(2, pwmResolution) -1);
        direction = server.arg("direction").toInt();

        EEPROM.put(0, pwmValue);
        EEPROM.put(sizeof(pwmValue), direction);
        EEPROM.commit();

        server.send(200, "text/html", "<html><body><h1>Settings Saved</h1><a href=\"/\">Go Back</a></body></html>");
    } else {
        server.send(400, "text/html", "<html><body><h1>Invalid Input</h1><a href=\"/\">Go Back</a></body></html>");
    }
}

void setup() {

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
    //driver.begin(&Wire, HBRIDGE_I2C_ADDR, 2, 1, 100000L);

    AtomS3.Display.setTextColor(WHITE);
    AtomS3.Display.setTextSize(3);
    AtomS3.Display.clear();
    tempus = millis();
    int pwmValue = map(targetRPM, 0, maxRPM, 0, pow(2, pwmResolution) -1);
}

void loop() {
    server.handleClient();

    bool btn_status = sensor.getButtonStatus();
    if (last_btn != btn_status) {
        if (!btn_status) {
            mstatus = mstatus + 1;
            if (mstatus == 5) mstatus = 0;
            AtomS3.Display.clear();
            AtomS3.Display.drawString(String(pwmValue), 10, 100);
            //AtomS3.Display.drawString(String(mstatus), 10, 100);
            newpress = true;
        }
        last_btn = btn_status;
    }

switch (mstatus) {

        case 0: //run motor
        { 
            if (newpress) {
                digitalWrite(directionPin, direction ? HIGH : LOW);
                digitalWrite(startStopPin, HIGH); // Start the motor
                AtomS3.Display.drawString("Running", 5, 0);
                newpress = false;
            }
            //if (millis() - tempus >= direction) // to be set by adjustment (100)
            //{
                //AtomS3.Display.drawString("Running", 5, 0);
                AtomS3.Display.drawString(String(pwmValue), 10, 30);
                AtomS3.Display.drawString(String(direction), 10, 60);
                
                ledcWrite(pwmChannel, pwmValue); // Set the PWM value
                
                
                //driver.setDriverDirection(HBRIDGE_FORWARD); // Set peristaltic pump in forward to take out BR content
                //driver.setDriverDirection(HBRIDGE_BACKWARD)
                //driver.setDriverSpeed8Bits(127); //Run pump in half speed
                //delay(ontime); // to be set by adjustment (30)
                //driver.setDriverDirection(HBRIDGE_STOP);
                //driver.setDriverSpeed8Bits(0);  //Stop pump
                //tempus = millis();
            //}
            break;
        } // end of case 0

        case 1: // read encoder for ON time in ms
        {
            signed short int encoder_value = sensor.getEncoderValue();
            //ontime = encoder_value;

            if (newpress) {
                //digitalWrite(startStopPin, LOW); // Stop the motor
                AtomS3.Display.drawString("Speed", 5, 0);
                AtomS3.Display.drawString(String(pwmValue), 10, 30);
                //AtomS3.Display.drawString(String(targetRPM), 10, 30);
                last_value = encoder_value; // Update the last value
                //pwmValue = map(targetRPM, 0, maxRPM, 0, pow(2, pwmResolution) -1);
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

                //relative_change = (encoder_value - last_value)*2; // Calculate the relative change
                
                

                AtomS3.Display.setTextColor(BLACK);
                //AtomS3.Display.drawString(String(targetRPM), 10, 30); // Clear the previous value
                AtomS3.Display.drawString(String(pwmValue), 10, 30); // Clear the previous value
                AtomS3.Display.setTextColor(WHITE);
                targetRPM = targetRPM + relative_change; // Update the value

                pwmValue = pwmValue + relative_change;
                ledcWrite(pwmChannel, pwmValue); // Set the PWM value
                AtomS3.Display.drawString(String(pwmValue), 10, 30); // Display the updated change
                //AtomS3.Display.drawString(String(targetRPM), 10, 30); // Display the updated change
                //pwmValue = map(targetRPM, 0, maxRPM, 0, pow(2, pwmResolution) -1);
                //float a0 = 9.50771;
                //float a1 = 2.79886;
                //float a2 = 5584.01;
                //float a3 = 257.21;
                //float a4 = 7.59115;
                //pwmValue = a3+((a0-a3) / (pow((1+pow((targetRPM/a2), a1)), a4)));
                //a3+((a0-a3)/(1+((x/a2)^a1))^a4)
                //pwmValue = a3+((a0-a3)/(1+((targetRPM/a2)^a1))^a4);
                //AtomS3.Display.drawString(String(pwmValue), 10, 100);

                //last_value = encoder_value; // Update the last value
            }
            delay(20);
            break;
        } // end of case 1

        case 2: // read encoder for direction in ms
        {
            signed short int encoder_value = sensor.getEncoderValue();
            //direction = encoder_value * 100;

            if (newpress) {
                AtomS3.Display.drawString("DIR", 5, 0);
                AtomS3.Display.drawString(String(direction), 10, 60);
                last_value = encoder_value; // Update the last value
                newpress = false;
            }

            if (last_value != encoder_value) {
                //int relative_change = encoder_value - last_value; // Calculate the relative change
                AtomS3.Display.setTextColor(BLACK);
                AtomS3.Display.drawString(String(direction), 10, 60);
                AtomS3.Display.setTextColor(WHITE);
                //direction != direction; // Update the value
                
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
                //AtomS3.Display.drawString(String(targetRPM), 10, 30);
                AtomS3.Display.drawString(String(direction), 10, 60);
                AtomS3.Display.drawString(String(pwmValue), 10, 100);
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
                AtomS3.Display.drawString("Saved", 30, 100);
                

            }
            break;    
        } // end of case 3

    

    } // end of switch cases
}