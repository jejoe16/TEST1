#include <Arduino.h>
#include <Wire.h>
#include <DFRobot_RGBLCD1602.h>
#include <SparkFun_LIS2DH12.h>
#include <SerialRFID.h>

// Dine specifikke pins på ESP32-S3
#define SDA_PIN 13
#define SCL_PIN 14

#define RFIDRX 18
#define RFIDTX 15

#define GPSRX 7
#define GPSTX 8
#define GPS1PPS 9

#define LTERX 35
#define LTETX 36
#define LTERST 10
#define LTEPWR 11

#define LRWRX 37
#define LRWTX 38
#define LRWRES 12

#define BUZZER_PIN 6 

#define VBATT 4
#define PSTAT 5

// Initialisering
// 0x2D er RGB-adressen for V2.0 af dette display
DFRobot_RGBLCD1602 lcd(0x2D, 16, 2); 

SPARKFUN_LIS2DH12 accel;

// Opsætning af RFID med Hardware Serial (Serial1)
SerialRFID rfid(Serial1); 

// Variabel til at gemme det læste RFID tag
char tag[SIZE_TAG_ID];

void setup() {
    Serial.begin(115200);
    
    // Start Hardware Serial1 til RFID (Baudrate, Konfiguration, RX pin, TX pin)
    Serial1.begin(9600, SERIAL_8N1, RFIDRX, RFIDTX);
    
    delay(2000);
    Serial.println(">> System Starter...");

    // 1. Start I2C bussen
    Wire.begin(SDA_PIN, SCL_PIN);

    // 2. Start LCD
    lcd.init();
    lcd.setRGB(100, 50, 255); 
    lcd.setCursor(0, 0);
    lcd.print("System Starter...");
    lcd.setBacklight(true);

    // 3. Start SparkFun Accelerometer
    if (accel.begin(0x18, Wire) == false) {
        Serial.println("LIS2DH12 ikke fundet!");
        lcd.setCursor(0, 1);
        lcd.print("Sensor Fejl!");
    } else {
        Serial.println("LIS2DH12 online!");
        lcd.setCursor(0, 1);
        lcd.print("Sensor OK!");
    }
    
    delay(1500);
    lcd.clear();
}

void loop() {
    // --- 1. Læs og Vis RFID ---
    if (rfid.readTag(tag, sizeof(tag))) {
        // Skriv til Serial Monitor
        Serial.print("RFID Tag: ");
        Serial.println(tag);

        // Skriv til LCD
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Scannet Tag:");
        lcd.setCursor(0, 1);
        lcd.print(tag);

        // Vent 2 sekunder så man kan nå at læse det på skærmen
        delay(2000);
        
        // Ryd skærmen inden accelerometeret tager over igen
        lcd.clear();
    }

    // --- 2. Læs Accelerometer ---
    if (accel.available()) {
        float x = accel.getX();
        float y = accel.getY();
        float z = accel.getZ();

        // Skriv til LCD - Linje 1
        lcd.setCursor(0, 0);
        lcd.print("X:"); lcd.print((int)x);
        lcd.print(" Y:"); lcd.print((int)y);
        lcd.print("    "); // Slet overskydende tegn

        // Skriv til LCD - Linje 2
        lcd.setCursor(0, 1);
        lcd.print("Z:"); lcd.print((int)z);
        lcd.print(" mg        ");
    }

    delay(200); // Opdateringshastighed
}