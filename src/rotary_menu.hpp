#pragma once

#include "util.hpp"

#include <Adafruit_SSD1306.h>
#include <Arduino.h>

#include <functional>
#include <vector>
#include <optional>

namespace mocca {
    class rotary_time_input {
      public:
        void set_current_time(uint32_t seconds, const char* default_option);
        bool is_on_default_option() const;
        uint32_t get_current_time() const;

        void set_time_step(uint32_t seconds);

        void on_encoder_changed(int delta);

        void draw(Adafruit_SSD1306* display, const box& area);

      private:
        String _default_option;
        bool _on_default_option = false;
        uint32_t _step = 1;
        uint32_t _seconds = 0;
    };

    using rotary_menu_callback = std::function<void(void)>;

    struct rotary_menu_option {
        String display_text;
        rotary_menu_callback callback;
    };

    class rotary_menu {
      public:
        void set_menu_options(std::vector<rotary_menu_option>&& options, uint32_t selected_option = 0);

        void on_encoder_changed(int delta);
        void on_encoder_clicked();

        void draw(Adafruit_SSD1306* display, const box& area);

      private:
        std::vector<rotary_menu_option> _menu_options;
        size_t _max_option_length = 0;
        uint32_t _selected_option = 0;
    };
} // namespace mocca