#pragma once

#include "util.hpp"

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

namespace mocca {
    class rotary_time_input {
      public:
        void set_current_time(uint32_t seconds);
        uint32_t get_current_time() const;

        void set_time_step(uint32_t seconds);

        void on_encoder_changed(int delta);

        void draw(Adafruit_SSD1306* display, const box& area);

      private:
        uint32_t _step = 1;
        uint32_t _seconds = 0;
    };
} // namespace mocca