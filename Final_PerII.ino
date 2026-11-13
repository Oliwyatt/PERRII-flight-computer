#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ICP101xx.h>

// ==========================================
// OLED CONFIGURATION
// ==========================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    -1
);

// ==========================================
// PIN DEFINITIONS (ESP32-C3 MINI)
// ==========================================

const int CH4_PIN = ;
const int NH3_PIN = ;

const int I2C_SDA = ;
const int I2C_SCL = ;

// Optional heartbeat LED
const int LED_PIN = -1;

// ==========================================
// SENSOR OBJECTS
// ==========================================

ICP101xx ICP(Wire);

bool icp_initialized = false;

// ==========================================
// FLIGHT LOGGING
// ==========================================

struct FlightRecord {
    uint32_t time_ms;
    float altitude;
    float apogee;
    int ch4;
    int nh3;
};

const int MAX_RECORDS = 1200; // 20 minutes @ 1Hz

FlightRecord flightLog[MAX_RECORDS];
int logIndex = 0;

// ==========================================
// GLOBALS
// ==========================================

float apogee = 0.0;
float launchPressure = -1.0;

unsigned long lastSystemUpdate = 0;
const uint32_t updateInterval = 200;

// ==========================================
// SETUP
// ==========================================

void setup() {

    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);

    Wire.begin(I2C_SDA, I2C_SCL);

    // OLED
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        display.display();
    }

    // ICP101xx
    if (ICP.begin() == 0) {
        ICP.start();
        icp_initialized = true;
        Serial.println("ICP101xx OK");
    } else {
        Serial.println("ICP101xx FAILED");
    }

    // Allow pressure sensor to stabilize
    delay(3000);

    if (icp_initialized) {

        float pressurePa;
        float tempC;

        if (ICP.getData(pressurePa, tempC) == 0) {

            launchPressure = pressurePa / 100.0;

            Serial.print("Launch pressure = ");
            Serial.print(launchPressure);
            Serial.println(" hPa");
        }
    }
}

// ==========================================
// LOOP
// ==========================================

void loop() {

    if (millis() - lastSystemUpdate >= updateInterval) {

        lastSystemUpdate = millis();

        // -----------------------------
        // Read Gas Sensors
        // -----------------------------

        int ch4_val = analogRead(CH4_PIN);
        int nh3_val = analogRead(NH3_PIN);

        // -----------------------------
        // Read Pressure Sensor
        // -----------------------------

        float pressure_hPa = 0;
        float temp_C = 0;

        if (icp_initialized) {

            float pressurePa;

            if (ICP.getData(pressurePa, temp_C) == 0) {

                pressure_hPa = pressurePa / 100.0;
            }
        }

        // -----------------------------
        // Calculate Relative Altitude
        // -----------------------------

        float altitude = 0;

        if (launchPressure > 0 && pressure_hPa > 0) {

            altitude =
                44330.0 *
                (1.0 - pow(pressure_hPa / launchPressure, 0.1903));
        }

        // -----------------------------
        // Update Apogee
        // -----------------------------

        if (altitude > apogee) {
            apogee = altitude;
        }

        // -----------------------------
        // Log Data
        // -----------------------------

        if (logIndex < MAX_RECORDS) {

            flightLog[logIndex].time_ms = millis();
            flightLog[logIndex].altitude = altitude;
            flightLog[logIndex].apogee = apogee;
            flightLog[logIndex].ch4 = ch4_val;
            flightLog[logIndex].nh3 = nh3_val;

            logIndex++;
        }

        // -----------------------------
        // Update OLED
        // -----------------------------

        updateScreen(
            ch4_val,
            nh3_val,
            pressure_hPa,
            altitude,
            apogee
        );

        // -----------------------------
        // Heartbeat LED
        // -----------------------------

        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    // Optional:
    // Type D in Serial Monitor to dump the log.

    if (Serial.available()) {

        char c = Serial.read();

        if (c == 'D' || c == 'd') {

            dumpFlightLog();
        }
    }
}

// ==========================================
// OLED DISPLAY
// ==========================================

void updateScreen(
    int ch4,
    int nh3,
    float pressure,
    float altitude,
    float apogee
) {

    display.clearDisplay();

    display.setCursor(0, 0);
    display.println("FLIGHT MODE");

    display.drawLine(0, 10, 128, 10, WHITE);

    display.setCursor(0, 15);
    display.print("CH4: ");
    display.println(ch4);

    display.print("NH3: ");
    display.println(nh3);

    display.print("P:");
    display.print(pressure, 1);
    display.println(" hPa");

    display.drawLine(0, 40, 128, 40, WHITE);

    display.setCursor(0, 45);

    display.print("Alt:");
    display.print(altitude, 1);
    display.println("m");

    display.print("Apo:");
    display.print(apogee, 1);
    display.println("m");

    display.display();
}

// ==========================================
// LOG DUMP
// ==========================================

void dumpFlightLog() {

    Serial.println();
    Serial.println("TIME_MS,ALTITUDE_M,APOGEE_M,CH4,NH3");

    for (int i = 0; i < logIndex; i++) {

        Serial.print(flightLog[i].time_ms);
        Serial.print(",");

        Serial.print(flightLog[i].altitude, 2);
        Serial.print(",");

        Serial.print(flightLog[i].apogee, 2);
        Serial.print(",");

        Serial.print(flightLog[i].ch4);
        Serial.print(",");

        Serial.println(flightLog[i].nh3);
    }

    Serial.println("END OF LOG");
}