#pragma once

#include "persistent_data.hpp"
#include "rotary_time_input.hpp"
#include "util.hpp"

#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h>
#include <OneButton.h>

namespace mocca {
    enum class state {
        sleep,
        idle,
        wake_set,
        menu,
        brew,
    };

    class mocca_wake {
      public:
        mocca_wake(Stream& log);

        bool init(int encoder_pin_a, int encoder_pin_b, int encoder_button_pin, int persistent_data_addr);

        void step();

      private:
        void draw_init_screen(const char* step_name, size_t step_index, size_t step_count);
        void draw_notification_bar(const box& area, box* out_content_area);
        void draw_sleep_screen();
        void draw_idle_screen();
        void draw_wake_set_screen();

        void save_persistent_data();

        void on_encoder_changed(int delta);
        void on_encoder_button_clicked();
        void on_encoder_button_double_clicked();
        void on_encoder_button_long_pressed();
        void on_any_input();

        void set_brew_time(uint32_t secs);

        void transition_to_state(state new_state);

        Stream& _log;

        int _persistent_data_addr = 0;
        persistent_data _data;

        Adafruit_SSD1306 _display;
        int64_t _last_encoder_count = 0;
        ESP32Encoder _encoder;
        OneButton _encoder_button;
        uint32_t _last_input_time = 0;

        rotary_time_input _time_input;

        Timezone _timezone;

        state _state = state::idle;
    };
} // namespace mocca
