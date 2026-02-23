#include <Arduino.h>
#include <Wire.h>
#include <DFRobot_RGBLCD1602.h>
#include <SparkFun_LIS2DH12.h>

// Dine specifikke pins på ESP32-S3
#define SDA_PIN 13
#define SCL_PIN 14

// Initialisering
// 0x2D er RGB-adressen for V2.0 af dette display
DFRobot_RGBLCD1602 lcd(0x2D, 16, 2); 
SPARKFUN_LIS2DH12 accel;

void setup() {
    Serial.begin(115200);
    delay(2000);

    // 1. Start I2C bussen
    Wire.begin(SDA_PIN, SCL_PIN);

    // 2. Start LCD
    lcd.init();
    // Sæt farven (R, G, B) - her sat til hvid (255, 255, 255)
    lcd.setRGB(255, 255, 255); 
    lcd.setCursor(0, 0);
    lcd.print("System Starter...");
    lcd.setBacklight(true);

    // 3. Start SparkFun Accelerometer
    // Vi bruger 0x18 som din scanner fandt
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
    if (accel.available()) {
        // Læs værdier (i mg)
        float x = accel.getX();
        float y = accel.getY();
        float z = accel.getZ();

        // Skriv til Serial Monitor
        Serial.print("X: "); Serial.print(x, 0);
        Serial.print(" Y: "); Serial.print(y, 0);
        Serial.print(" Z: "); Serial.println(z, 0);

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