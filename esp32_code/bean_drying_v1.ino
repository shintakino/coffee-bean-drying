#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RTClib.h>
#include <ESP32Servo.h>
#include <Preferences.h>

// ------------------- DHT22 --------------------
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ------------------- LCD ---------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ------------------- RTC ---------------------
RTC_DS1307 rtc;

// ------------------- Servo ------------------
#define SERVO_PIN 27
Servo myServo;
int servoPos = 0;

// ------------------- Relays ------------------
#define RELAY1 16  // Heater
#define RELAY2 17  // Fans
#define RELAY3 18  // Humidifier
#define RELAY4 19  // Servo Control Relay

// ------------------- Push Buttons ------------
#define BTN_MODE 5     // Button to switch LCD modes
#define BTN_UP 12      // Button to increase values
#define BTN_DOWN 14    // Button to decrease values
#define BTN_TIMER 26   // Button to enter timer settings

// ------------------- Preferences -------------
Preferences prefs;

// ------------------- Variables ----------------
float temp, humi;
float tempMin = 20, tempMax = 30;
float humiMin = 40, humiMax = 60;
int mode = 0;
int editMode = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 200;
unsigned long lastSaveTime = 0;
const unsigned long SAVE_INTERVAL = 5000;

// Timer variables
int startHour = 8, startMin = 0;
int stopHour = 18, stopMin = 0;
bool timerActive = false;
bool servoEnabled = false;
bool settingsChanged = false;

// LCD Management
unsigned long lastLCDUpdate = 0;
const unsigned long LCD_UPDATE_INTERVAL = 500;
bool blinkState = false;
unsigned long lastBlinkTime = 0;
const unsigned long BLINK_INTERVAL = 300;
int lastMode = -1;
int lastEditMode = -1;

void setup() {
    Serial.begin(115200);
    prefs.begin("env-control", false);
    loadSettings();
    
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        lcd.init();
        lcd.backlight();
        lcd.print("RTC not found!");
        delay(2000);
    }
    
    dht.begin();
    lcd.init();
    lcd.backlight();
    
    myServo.attach(SERVO_PIN);
    myServo.write(0);
    
    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    pinMode(RELAY3, OUTPUT);
    pinMode(RELAY4, OUTPUT);
    
    pinMode(BTN_MODE, INPUT_PULLUP);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_TIMER, INPUT_PULLUP);
    
    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);
    digitalWrite(RELAY3, LOW);
    digitalWrite(RELAY4, LOW);
    
    lcd.clear();
    lcd.print("Environmental");
    lcd.setCursor(0, 1);
    lcd.print("Control System");
    delay(2000);
}

void loop() {
    DateTime now = rtc.now();
    temp = dht.readTemperature();
    humi = dht.readHumidity();
    
    if (!isnan(temp) && !isnan(humi)) {
        checkTimer(now);
        controlRelays();
        handleButtons();
        
        if (millis() - lastLCDUpdate > LCD_UPDATE_INTERVAL || editMode > 0) {
            updateLCD(now);
            lastLCDUpdate = millis();
        }
        
        if (settingsChanged && (millis() - lastDebounceTime > SAVE_INTERVAL)) {
            saveSettings();
            settingsChanged = false;
            lastSaveTime = millis();
        }
    } else {
        lcd.clear();
        lcd.print("Sensor Error!");
        delay(1000);
    }
    
    delay(100);
}

void loadSettings() {
    tempMin = prefs.getFloat("tempMin", 20.0);
    tempMax = prefs.getFloat("tempMax", 30.0);
    humiMin = prefs.getFloat("humiMin", 40.0);
    humiMax = prefs.getFloat("humiMax", 60.0);
    startHour = prefs.getInt("startHour", 8);
    startMin = prefs.getInt("startMin", 0);
    stopHour = prefs.getInt("stopHour", 18);
    stopMin = prefs.getInt("stopMin", 0);
    Serial.println("Settings loaded");
}

void saveSettings() {
    lcd.clear();
    lcd.print("Saving settings");
    delay(500);
    
    prefs.putFloat("tempMin", tempMin);
    prefs.putFloat("tempMax", tempMax);
    prefs.putFloat("humiMin", humiMin);
    prefs.putFloat("humiMax", humiMax);
    prefs.putInt("startHour", startHour);
    prefs.putInt("startMin", startMin);
    prefs.putInt("stopHour", stopHour);
    prefs.putInt("stopMin", stopMin);
    
    lcd.clear();
    lcd.print("Settings saved!");
    delay(1000);
}

void checkTimer(DateTime now) {
    int currentTime = now.hour() * 60 + now.minute();
    int startTime = startHour * 60 + startMin;
    int stopTime = stopHour * 60 + stopMin;
    
    if (startTime <= currentTime && currentTime < stopTime) {
        if (!timerActive) {
            timerActive = true;
            servoEnabled = true;
            digitalWrite(RELAY4, HIGH);
            myServo.write(90);
        }
    } else {
        if (timerActive) {
            timerActive = false;
            servoEnabled = false;
            digitalWrite(RELAY4, LOW);
            myServo.write(0);
        }
    }
}

void controlRelays() {
    digitalWrite(RELAY1, temp < tempMin ? HIGH : LOW);
    digitalWrite(RELAY2, temp > tempMax ? HIGH : LOW);
    digitalWrite(RELAY3, humi < humiMin ? HIGH : LOW);
}

void handleButtons() {
    // MODE button handling
    if (digitalRead(BTN_MODE) == LOW && millis() - lastDebounceTime > debounceDelay) {
        unsigned long pressStart = millis();
        while (digitalRead(BTN_MODE) == LOW && millis() - pressStart < 1000) {
            delay(50);
        }
        
        if (millis() - pressStart >= 1000) {
            // Long press - enter edit mode
            if (editMode == 0) {
                switch (mode) {
                    case 1: editMode = 1; break;
                    case 2: editMode = 3; break;
                    case 4: editMode = 5; break;
                }
            } else {
                editMode = 0;
            }
        } else {
            // Short press - change mode or edit parameter
            if (editMode == 0) {
                mode = (mode + 1) % 5;
            } else {
                switch (editMode) {
                    case 1: editMode = 2; break;
                    case 2: editMode = 0; break;
                    case 3: editMode = 4; break;
                    case 4: editMode = 0; break;
                    case 5: editMode = 6; break;
                    case 6: editMode = 7; break;
                    case 7: editMode = 8; break;
                    case 8: editMode = 0; break;
                }
            }
        }
        lastDebounceTime = millis();
    }

    // TIMER button handling
    if (digitalRead(BTN_TIMER) == LOW && millis() - lastDebounceTime > debounceDelay) {
        unsigned long pressStart = millis();
        while (digitalRead(BTN_TIMER) == LOW && millis() - pressStart < 1000) {
            delay(50);
        }
        
        if (millis() - pressStart >= 1000) {
            // Long press - toggle servo
            servoEnabled = !servoEnabled;
            digitalWrite(RELAY4, servoEnabled ? HIGH : LOW);
            myServo.write(servoEnabled ? 90 : 0);
            lcd.clear();
            lcd.print("Servo manually");
            lcd.setCursor(0, 1);
            lcd.print(servoEnabled ? "ENABLED" : "DISABLED");
            delay(1000);
        } else {
            // Short press - show timer
            if (editMode == 0) mode = 4;
        }
        lastDebounceTime = millis();
    }

    // UP/DOWN buttons
    if (editMode > 0) {
        if (digitalRead(BTN_UP) == LOW && millis() - lastDebounceTime > debounceDelay) {
            adjustValues(true);
            settingsChanged = true;
            lastDebounceTime = millis();
        }
        if (digitalRead(BTN_DOWN) == LOW && millis() - lastDebounceTime > debounceDelay) {
            adjustValues(false);
            settingsChanged = true;
            lastDebounceTime = millis();
        }
    }
}

void adjustValues(bool increase) {
    const float tempIncrement = 0.5;
    
    switch (editMode) {
        case 1: tempMin = constrain(increase ? tempMin + tempIncrement : tempMin - tempIncrement, -10, tempMax-1); break;
        case 2: tempMax = constrain(increase ? tempMax + tempIncrement : tempMax - tempIncrement, tempMin+1, 50); break;
        case 3: humiMin = constrain(increase ? humiMin + 1 : humiMin - 1, 0, humiMax-1); break;
        case 4: humiMax = constrain(increase ? humiMax + 1 : humiMax - 1, humiMin+1, 100); break;
        case 5: startHour = (startHour + (increase ? 1 : 23)) % 24; break;
        case 6: startMin = (startMin + (increase ? 1 : 59)) % 60; break;
        case 7: stopHour = (stopHour + (increase ? 1 : 23)) % 24; break;
        case 8: stopMin = (stopMin + (increase ? 1 : 59)) % 60; break;
    }
}

String padZero(int num) {
    return num < 10 ? "0" + String(num) : String(num);
}

void updateLCD(DateTime now) {
    if (mode != lastMode || editMode != lastEditMode) {
        lcd.clear();
        lastMode = mode;
        lastEditMode = editMode;
    }
    
    if (editMode > 0) {
        if (millis() - lastBlinkTime > BLINK_INTERVAL) {
            blinkState = !blinkState;
            lastBlinkTime = millis();
        }
        
        lcd.setCursor(0, 0);
        lcd.print("Edit: ");
        
        lcd.setCursor(0, 1);
        switch (editMode) {
            case 1: 
                lcd.print("Temp Min:");
                if (!blinkState) {
                    lcd.setCursor(10, 1);
                    lcd.print(tempMin);
                }
                break;
            case 2:
                lcd.print("Temp Max:");
                if (!blinkState) {
                    lcd.setCursor(10, 1);
                    lcd.print(tempMax);
                }
                break;
            case 3:
                lcd.print("Humi Min:");
                if (!blinkState) {
                    lcd.setCursor(10, 1);
                    lcd.print(humiMin);
                }
                break;
            case 4:
                lcd.print("Humi Max:");
                if (!blinkState) {
                    lcd.setCursor(10, 1);
                    lcd.print(humiMax);
                }
                break;
            case 5:
                lcd.print("Start Hour:");
                if (!blinkState) {
                    lcd.setCursor(12, 1);
                    lcd.print(padZero(startHour));
                }
                break;
            case 6:
                lcd.print("Start Min:");
                if (!blinkState) {
                    lcd.setCursor(12, 1);
                    lcd.print(padZero(startMin));
                }
                break;
            case 7:
                lcd.print("Stop Hour:");
                if (!blinkState) {
                    lcd.setCursor(12, 1);
                    lcd.print(padZero(stopHour));
                }
                break;
            case 8:
                lcd.print("Stop Min:");
                if (!blinkState) {
                    lcd.setCursor(12, 1);
                    lcd.print(padZero(stopMin));
                }
                break;
        }
    } else {
        switch (mode) {
            case 0:
                lcd.setCursor(0, 0);
                lcd.print("Temp: ");
                lcd.print(temp);
                lcd.print(" C");
                lcd.setCursor(0, 1);
                lcd.print("Humi: ");
                lcd.print(humi);
                lcd.print(" %");
                break;
            case 1:
                lcd.setCursor(0, 0);
                lcd.print("T-Range: ");
                lcd.setCursor(0, 1);
                lcd.print(tempMin);
                lcd.print("-");
                lcd.print(tempMax);
                lcd.print(" C");
                break;
            case 2:
                lcd.setCursor(0, 0);
                lcd.print("H-Range: ");
                lcd.setCursor(0, 1);
                lcd.print(humiMin);
                lcd.print("-");
                lcd.print(humiMax);
                lcd.print(" %");
                break;
            case 3:
                lcd.setCursor(0, 0);
                lcd.print("H:");
                lcd.print(digitalRead(RELAY1) ? "ON " : "OFF");
                lcd.print(" F:");
                lcd.print(digitalRead(RELAY2) ? "ON" : "OFF");
                lcd.setCursor(0, 1);
                lcd.print("Hm:");
                lcd.print(digitalRead(RELAY3) ? "ON " : "OFF");
                lcd.print(" S:");
                lcd.print(digitalRead(RELAY4) ? "ON" : "OFF");
                break;
            case 4:
                lcd.setCursor(0, 0);
                lcd.print(padZero(now.hour()));
                lcd.print(":");
                lcd.print(padZero(now.minute()));
                lcd.print(" ");
                lcd.print(servoEnabled ? "S:ON" : "S:OFF");
                lcd.setCursor(0, 1);
                lcd.print("T:");
                lcd.print(padZero(startHour));
                lcd.print(":");
                lcd.print(padZero(startMin));
                lcd.print("-");
                lcd.print(padZero(stopHour));
                lcd.print(":");
                lcd.print(padZero(stopMin));
                break;
        }
    }
}