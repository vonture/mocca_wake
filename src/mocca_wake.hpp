#pragma once

#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h>

namespace mocca {
    class mocca_wake {
      public:
        mocca_wake(Stream& log);

        bool init(int encoder_pin_a, int encoder_pin_b, int encoder_button_pin);

        void step();

      private:
        Stream& _log;
        Adafruit_SSD1306 _display;
        ESP32Encoder _encoder;
    };
} // namespace mocca
