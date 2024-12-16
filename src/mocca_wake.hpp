#pragma once

#include "persistent_data.hpp"

#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h>

namespace mocca {
    class mocca_wake {
      public:
        mocca_wake(Stream& log);

        bool init(int encoder_pin_a, int encoder_pin_b, int encoder_button_pin, int persistent_data_addr);

        void step();

      private:
        void draw_init_screen(const char* step_name, size_t step_index, size_t step_count);
        void save_persistent_data();

        Stream& _log;

        int _persistent_data_addr = 0;
        persistent_data _data;

        Adafruit_SSD1306 _display;
        ESP32Encoder _encoder;

        Timezone _timezone;
    };
} // namespace mocca
