#include <Arduino.h>
#include "mocca_wake.hpp"
#include <EEPROM.h>
#include <WiFi.h>

static mocca::mocca_wake wake(USBSerial);

static constexpr int i2c_sda_pin = 13;
static constexpr int i2c_scl_pin = 15;

static constexpr int encoder_pin_a = 1;
static constexpr int encoder_pin_b = 2;
static constexpr int encoder_button_pin = 3;

void setup() {
    USBSerial.begin(9600);

    delay(5000);
    USBSerial.println("USBSerial UP");

    Wire.setPins(i2c_sda_pin, i2c_scl_pin);

    if (!wake.init(encoder_pin_a, encoder_pin_b, encoder_button_pin)) {
        USBSerial.println("mocca_wake::init failed.");
    }
}

void loop() {
    wake.step();
}
