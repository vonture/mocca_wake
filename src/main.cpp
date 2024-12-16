#include "mocca_wake.hpp"

#include <Arduino.h>
#include <EEPROM.h>

static mocca::mocca_wake wake(USBSerial);

static constexpr int i2c_sda_pin = 13;
static constexpr int i2c_scl_pin = 15;

static constexpr int encoder_pin_a = 1;
static constexpr int encoder_pin_b = 2;
static constexpr int encoder_button_pin = 3;

static constexpr size_t eeprom_size = sizeof(mocca::persistent_data);
static constexpr int eeprom_persistent_data_addr = 0;

void setup() {
    USBSerial.begin(9600);

   if (! EEPROM.begin(eeprom_size)) {
        USBSerial.println("EEPROM::begin failed.");
   }

    Wire.setPins(i2c_sda_pin, i2c_scl_pin);

    if (!wake.init(encoder_pin_a, encoder_pin_b, encoder_button_pin, eeprom_persistent_data_addr)) {
        USBSerial.println("mocca_wake::init failed.");
    }
}

void loop() {
    wake.step();
}
