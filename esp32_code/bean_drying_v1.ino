#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RTClib.h>
#include <ESP32Servo.h>
#include <Preferences.h>  // ESP32 non-volatile storage

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
Preferences prefs;  // For storing settings in flash memory

// ------------------- Variables ----------------
float temp, humi;
float tempMin = 20, tempMax = 30;
float humiMin = 40, humiMax = 60;
int mode = 0;
int editMode = 0;  // 0: View mode, 1: Edit tempMin, 2: Edit tempMax, 3: Edit humiMin, 4: Edit humiMax, 5-8: Timer settings
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 200;  // Debounce time in milliseconds
unsigned long lastSaveTime = 0;     // Track when we last saved to EEPROM
const unsigned long SAVE_INTERVAL = 5000;  // Save settings after 5 seconds of inactivity

// Timer variables
int startHour = 8, startMin = 0;    // Default 8:00 AM
int stopHour = 18, stopMin = 0;     // Default 6:00 PM
bool timerActive = false;
bool servoEnabled = false;
bool settingsChanged = false;       // Flag to track if settings have changed

void setup() {
    Serial.begin(115200);
    
    // Open preferences with namespace "env-control"
    prefs.begin("env-control", false);
    
    // Load saved settings
    loadSettings();
    
    // Initialize RTC
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        lcd.clear();
        lcd.print("RTC not found!");
        lcd.setCursor(0, 1);
        lcd.print("Check wiring");
        delay(2000);
    }
    
    // Set RTC time if needed (run once, then comment out)
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); 
    
    dht.begin();
    lcd.init();
    lcd.backlight();
    
    // Initialize servo
    myServo.attach(SERVO_PIN);
    myServo.write(0);  // Initial position
    
    // Initialize relays
    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    pinMode(RELAY3, OUTPUT);
    pinMode(RELAY4, OUTPUT);
    
    // Initialize buttons
    pinMode(BTN_MODE, INPUT_PULLUP);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_TIMER, INPUT_PULLUP);
    
    // Initialize with relays off
    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);
    digitalWrite(RELAY3, LOW);
    digitalWrite(RELAY4, LOW);
    
    // Show welcome message
    lcd.clear();
    lcd.print("Environmental");
    lcd.setCursor(0, 1);
    lcd.print("Control System");
    delay(2000);
}

void loop() {
    // Get current time from RTC
    DateTime now = rtc.now();
    
    // Read sensor data
    temp = dht.readTemperature();
    humi = dht.readHumidity();
    
    if (!isnan(temp) && !isnan(humi)) {
        // Check timer for servo control
        checkTimer(now);
        
        // Control relays based on sensor readings
        controlRelays();
        
        // Handle button presses
        handleButtons();
        
        // Update LCD display
        updateLCD(now);
        
        // Save settings if they've changed and no button has been pressed for a while
        if (settingsChanged && (millis() - lastDebounceTime > SAVE_INTERVAL) && 
            (millis() - lastSaveTime > SAVE_INTERVAL)) {
            saveSettings();
            settingsChanged = false;
            lastSaveTime = millis();
        }
    } else {
        // Show error if sensor readings fail
        lcd.clear();
        lcd.print("Sensor Error!");
        delay(1000);
    }
    
    delay(100);
}

void loadSettings() {
    // Load temperature settings
    tempMin = prefs.getFloat("tempMin", 20.0);
    tempMax = prefs.getFloat("tempMax", 30.0);
    
    // Load humidity settings
    humiMin = prefs.getFloat("humiMin", 40.0);
    humiMax = prefs.getFloat("humiMax", 60.0);
    
    // Load timer settings
    startHour = prefs.getInt("startHour", 8);
    startMin = prefs.getInt("startMin", 0);
    stopHour = prefs.getInt("stopHour", 18);
    stopMin = prefs.getInt("stopMin", 0);
    
    Serial.println("Settings loaded from flash memory");
}

void saveSettings() {
    // Show saving message
    lcd.clear();
    lcd.print("Saving settings");
    lcd.setCursor(0, 1);
    lcd.print("Please wait...");
    
    // Save temperature settings
    prefs.putFloat("tempMin", tempMin);
    prefs.putFloat("tempMax", tempMax);
    
    // Save humidity settings
    prefs.putFloat("humiMin", humiMin);
    prefs.putFloat("humiMax", humiMax);
    
    // Save timer settings
    prefs.putInt("startHour", startHour);
    prefs.putInt("startMin", startMin);
    prefs.putInt("stopHour", stopHour);
    prefs.putInt("stopMin", stopMin);
    
    Serial.println("Settings saved to flash memory");
    
    // Show confirmation
    lcd.clear();
    lcd.print("Settings saved!");
    delay(1000);
}

void checkTimer(DateTime now) {
    // Check if current time is within the set time range
    int currentHour = now.hour();
    int currentMin = now.minute();
    
    // Calculate minutes since midnight for easier comparison
    int currentTimeMinutes = currentHour * 60 + currentMin;
    int startTimeMinutes = startHour * 60 + startMin;
    int stopTimeMinutes = stopHour * 60 + stopMin;
    
    // Check if timer should be active
    if (startTimeMinutes <= currentTimeMinutes && currentTimeMinutes < stopTimeMinutes) {
        if (!timerActive) {
            // Timer just became active, move servo
            timerActive = true;
            servoEnabled = true;
            digitalWrite(RELAY4, HIGH);
            myServo.write(90); // Move to 90 degrees when timer starts
        }
    } else {
        if (timerActive) {
            // Timer just became inactive, reset servo
            timerActive = false;
            servoEnabled = false;
            digitalWrite(RELAY4, LOW);
            myServo.write(0); // Return to 0 degrees when timer stops
        }
    }
}

void controlRelays() {
    // Heater: Turn ON if temperature is below minimum
    digitalWrite(RELAY1, temp < tempMin ? HIGH : LOW);
    
    // Fans: Turn ON if temperature is above maximum
    digitalWrite(RELAY2, temp > tempMax ? HIGH : LOW);
    
    // Humidifier: Turn ON if humidity is below minimum
    digitalWrite(RELAY3, humi < humiMin ? HIGH : LOW);
    
    // RELAY4 is controlled by the timer function
}

void handleButtons() {
    // MODE button: Switch between display modes (single click) or enter edit mode (long press)
    if (digitalRead(BTN_MODE) == LOW) {
        if (millis() - lastDebounceTime > debounceDelay) {
            // Check if this is a long press (more than 1 second)
            unsigned long pressStart = millis();
            while (digitalRead(BTN_MODE) == LOW) {
                if (millis() - pressStart > 1000) {
                    // Long press detected - enter edit mode for current display
                    if (editMode == 0) {
                        switch (mode) {
                            case 0: // Current readings - no edit mode
                                break;
                            case 1: // Temperature range
                                editMode = 1; // Start with tempMin
                                break;
                            case 2: // Humidity range
                                editMode = 3; // Start with humiMin
                                break;
                            case 3: // Relay status - no edit mode
                                break;
                            case 4: // Timer display
                                editMode = 5; // Start with start hour
                                break;
                        }
                    } else {
                        // If already in edit mode, exit
                        editMode = 0;
                    }
                    lastDebounceTime = millis();
                    return;
                }
                delay(50);
            }
            
            // If we get here, it was a short press
            if (editMode == 0) {
                // Cycle through display modes
                mode = (mode + 1) % 5;
            } else {
                // In edit mode, cycle through editable parameters
                switch (editMode) {
                    case 1: editMode = 2; break; // tempMin -> tempMax
                    case 2: editMode = 0; break; // tempMax -> exit
                    case 3: editMode = 4; break; // humiMin -> humiMax
                    case 4: editMode = 0; break; // humiMax -> exit
                    case 5: editMode = 6; break; // startHour -> startMin
                    case 6: editMode = 7; break; // startMin -> stopHour
                    case 7: editMode = 8; break; // stopHour -> stopMin
                    case 8: editMode = 0; break; // stopMin -> exit
                }
            }
            lastDebounceTime = millis();
        }
    }
    
    // TIMER button: Show timer display (single click) or toggle servo (long press)
    if (digitalRead(BTN_TIMER) == LOW) {
        if (millis() - lastDebounceTime > debounceDelay) {
            // Check if this is a long press (more than 1 second)
            unsigned long pressStart = millis();
            while (digitalRead(BTN_TIMER) == LOW) {
                if (millis() - pressStart > 1000) {
                    // Long press detected - manually toggle servo
                    servoEnabled = !servoEnabled;
                    digitalWrite(RELAY4, servoEnabled ? HIGH : LOW);
                    myServo.write(servoEnabled ? 90 : 0);
                    
                    // Show confirmation
                    lcd.clear();
                    lcd.print("Servo manually");
                    lcd.setCursor(0, 1);
                    lcd.print(servoEnabled ? "ENABLED" : "DISABLED");
                    delay(1000);
                    
                    lastDebounceTime = millis();
                    return;
                }
                delay(50);
            }
            
            // If we get here, it was a short press
            if (editMode == 0) {
                // Switch to timer display mode
                mode = 4;
            }
            lastDebounceTime = millis();
        }
    }
    
    // UP button: Increase values in edit mode
    if (digitalRead(BTN_UP) == LOW && editMode != 0) {
        if (millis() - lastDebounceTime > debounceDelay) {
            adjustValues(true);
            settingsChanged = true;
            lastDebounceTime = millis();
        }
    }
    
    // DOWN button: Decrease values in edit mode
    if (digitalRead(BTN_DOWN) == LOW && editMode != 0) {
        if (millis() - lastDebounceTime > debounceDelay) {
            adjustValues(false);
            settingsChanged = true;
            lastDebounceTime = millis();
        }
    }
}

void adjustValues(bool increase) {
    const float tempIncrement = 0.5;
    
    switch (editMode) {
        case 1:  // Edit tempMin
            tempMin = increase ? tempMin + tempIncrement : tempMin - tempIncrement;
            if (tempMin >= tempMax) tempMin = tempMax - 1;
            break;
        case 2:  // Edit tempMax
            tempMax = increase ? tempMax + tempIncrement : tempMax - tempIncrement;
            if (tempMax <= tempMin) tempMax = tempMin + 1;
            break;
        case 3:  // Edit humiMin
            humiMin = increase ? humiMin + tempIncrement : humiMin - tempIncrement;
            if (humiMin >= humiMax) humiMin = humiMax - 1;
            if (humiMin < 0) humiMin = 0;
            break;
        case 4:  // Edit humiMax
            humiMax = increase ? humiMax + tempIncrement : humiMax - tempIncrement;
            if (humiMax <= humiMin) humiMax = humiMin + 1;
            if (humiMax > 100) humiMax = 100;
            break;
        case 5:  // Edit Start Hour
            startHour = increase ? (startHour + 1) % 24 : (startHour + 23) % 24;
            break;
        case 6:  // Edit Start Minute
            startMin = increase ? (startMin + 1) % 60 : (startMin + 59) % 60;
            break;
        case 7:  // Edit Stop Hour
            stopHour = increase ? (stopHour + 1) % 24 : (stopHour + 23) % 24;
            break;
        case 8:  // Edit Stop Minute
            stopMin = increase ? (stopMin + 1) % 60 : (stopMin + 59) % 60;
            break;
    }
}

String padZero(int num) {
    // Add leading zero for single-digit numbers
    if (num < 10) {
        return "0" + String(num);
    }
    return String(num);
}

void updateLCD(DateTime now) {
    lcd.clear();
    
    if (editMode > 0) {
        // In edit mode, display and highlight the value being edited
        lcd.print("Edit Mode:");
        lcd.setCursor(0, 1);
        
        switch (editMode) {
            case 1:
                lcd.print("Temp Min: " + String(tempMin));
                break;
            case 2:
                lcd.print("Temp Max: " + String(tempMax));
                break;
            case 3:
                lcd.print("Humi Min: " + String(humiMin));
                break;
            case 4:
                lcd.print("Humi Max: " + String(humiMax));
                break;
            case 5:
                lcd.print("Start Hour: " + padZero(startHour));
                break;
            case 6:
                lcd.print("Start Min: " + padZero(startMin));
                break;
            case 7:
                lcd.print("Stop Hour: " + padZero(stopHour));
                break;
            case 8:
                lcd.print("Stop Min: " + padZero(stopMin));
                break;
        }
        
    } else {
        // Normal display modes
        if (mode == 0) {
            // Current readings
            lcd.print("Temp: " + String(temp) + " C");
            lcd.setCursor(0, 1);
            lcd.print("Humi: " + String(humi) + " %");
        } else if (mode == 1) {
            // Temperature range
            lcd.print("T-Range: ");
            lcd.setCursor(0, 1);
            lcd.print(String(tempMin) + "-" + String(tempMax) + " C");
        } else if (mode == 2) {
            // Humidity range
            lcd.print("H-Range: ");
            lcd.setCursor(0, 1);
            lcd.print(String(humiMin) + "-" + String(humiMax) + " %");
        } else if (mode == 3) {
            // Relay status
            lcd.print("H:");
            lcd.print(digitalRead(RELAY1) ? "ON " : "OFF");
            lcd.print(" F:");
            lcd.print(digitalRead(RELAY2) ? "ON" : "OFF");
            lcd.setCursor(0, 1);
            lcd.print("Hm:");
            lcd.print(digitalRead(RELAY3) ? "ON " : "OFF");
            lcd.print(" S:");
            lcd.print(digitalRead(RELAY4) ? "ON" : "OFF");
        } else if (mode == 4) {
            // Timer & Clock
            lcd.print(padZero(now.hour()) + ":" + padZero(now.minute()) + " " + 
                      (servoEnabled ? "S:ON" : "S:OFF"));
            lcd.setCursor(0, 1);
            lcd.print("T:" + padZero(startHour) + ":" + padZero(startMin) + 
                      "-" + padZero(stopHour) + ":" + padZero(stopMin));
        }
    }
}