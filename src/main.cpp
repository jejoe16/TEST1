#include <Arduino.h>
#include <Wire.h>
#include <DFRobot_RGBLCD1602.h>
#include <SparkFun_LIS2DH12.h>
#include <SoftwareSerial.h>
#include <SerialRFID.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include "LTEModem.h"

// Fortæl TinyGSM, at vi bruger et u-blox modem (LEXI deler AT-kommandoer med SARA-serien)

// Dine specifikke pins på ESP32-S3
#define SDA_PIN 13
#define SCL_PIN 14

#define RFIDRX 18
#define RFIDTX 15

#define GPSRX 7
#define GPSTX 8
#define GPS1PPS 9

#define LTERX 1
#define LTETX 2
#define LTEPWR 3

#define LRWRX 37
#define LRWTX 38
#define LRWRES 12

#define BUZZER_PIN 6 
#define VBATT 4
#define PSTAT 5

LTEModem modem(Serial1, &Serial);

// --- 1. Initialisering af I2C Hardware ---
DFRobot_RGBLCD1602 lcd(0x2D, 16, 2); 
SPARKFUN_LIS2DH12 accel;

// --- 2. Opsætning af Serielle Porte ---
// RFID på SoftwareSerial (9600 baud)
SoftwareSerial rfidSerial(RFIDRX, RFIDTX);
SerialRFID rfid(rfidSerial); 

// GPS på Hardware Serial2
SFE_UBLOX_GNSS myGNSS;


// Variabel til RFID tag
char tag[SIZE_TAG_ID];

// Funktion til at tænde u-blox LEXI-R10 modem
void powerOnModem() {
    Serial.println(">> Forsøger at tænde LEXI-R10 Modem...");
    pinMode(LTEPWR, OUTPUT);
    
    // Baseret på din kode: Puls sekvens
    digitalWrite(LTEPWR, HIGH);
    delay(1000);
    digitalWrite(LTEPWR, LOW);
    
    delay(1000); 
    Serial.println(">> Modem power cycle færdig. Klar til AT kommandoer.");
    Serial.println(">> Skriv 'AT' i Serial Monitor for at teste forbindelse.");
}

void setup() {
    // Debug Monitor (UART0)
    Serial.begin(115200);
    delay(2000);
    Serial.println(">> System Starter...");

    // Start Serielle forbindelser
    Serial1.begin(115200, SERIAL_8N1, LTERX, LTETX); // LTE Modem
    Serial2.begin(9600, SERIAL_8N1, GPSRX, GPSTX);   // GPS
    rfidSerial.begin(9600);                          // RFID

    // Start I2C bussen og LCD
    Wire.begin(SDA_PIN, SCL_PIN);
    lcd.init();
    lcd.setRGB(100, 50, 255); 
    lcd.setCursor(0, 0);
    lcd.print("System Starter...");
    lcd.setBacklight(true);

    // Start Accelerometer
    if (accel.begin(0x18, Wire) == false) {
        Serial.println("LIS2DH12 ikke fundet!");
    } else {
        Serial.println("LIS2DH12 online!");
    }

    // Start GPS
    if (myGNSS.begin(Serial2) == false) {
        Serial.println("u-blox GNSS ikke fundet på Serial2!");
    } else {
        Serial.println("u-blox GNSS online!");
        myGNSS.setUART1Output(COM_TYPE_UBX); 
    }

    // Tænd og start LTE Modem
    powerOnModem();
    
    
        // Full init: handshake → SIM → MNO → APN → register → attach → PDP
    if (!modem.init("your.apn.here", 100)) {  // 100 = Standard Europe
        Serial.println("Modem init failed!");
        return;
    }
    
    delay(2000);
    lcd.clear();
}

void loop() {

    // --- 1. Læs RFID (via SoftwareSerial) ---
    
    if (rfid.readTag(tag, sizeof(tag))) {
        Serial.print("RFID Tag scannet: ");
        Serial.println(tag);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Scannet Tag:");
        lcd.setCursor(0, 1);
        lcd.print(tag);

        delay(2000); // Vis på skærmen i 2 sekunder
        lcd.clear();
    }

    // --- 2. Læs GPS (via Serial2) ---
    if (myGNSS.getPVT()) {
        float latitude = myGNSS.getLatitude() / 10000000.0;
        float longitude = myGNSS.getLongitude() / 10000000.0;
        
        Serial.print("GPS -> Lat: ");
        Serial.print(latitude, 6);
        Serial.print(" | Lon: ");
        Serial.println(longitude, 6);
    }

    // --- 3. Læs Accelerometer (via I2C) ---
    if (accel.available()) {
        float x = accel.getX();
        float y = accel.getY();
        float z = accel.getZ();

        lcd.setCursor(0, 0);
        lcd.print("X:"); lcd.print((int)x);
        lcd.print(" Y:"); lcd.print((int)y);
        lcd.print("    "); 

        lcd.setCursor(0, 1);
        lcd.print("Z:"); lcd.print((int)z);
        lcd.print(" mg        ");
    }

    delay(200); // Kør loop med 5 Hz
    
}