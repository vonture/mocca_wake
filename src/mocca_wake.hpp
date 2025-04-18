#pragma once

#include "config_web_server.hpp"
#include "persistent_data.hpp"
#include "rotary_menu.hpp"
#include "util.hpp"

#include <Adafruit_SSD1306.h>
#include <ESP32Encoder.h>
#include <OneButton.h>

namespace mocca {
    enum class state {
        sleep,
        idle,
        wake_set,
        time_set,
        menu,
        status,
        brew,
    };

    class mocca_wake {
      public:
        mocca_wake(Stream& log);

        bool init(int encoder_pin_a, int encoder_pin_b, int encoder_button_pin, int water_switch_pin,
                  int pot_switch_pin, int boiler_ssr_pin, int persistent_data_addr);

        void step();

      private:
        void draw_init_screen(const char* step_name, size_t step_index, size_t step_count);
        void draw_notification_bar(const box& area, box* out_content_area);
        void draw_time_to_idle_bar();
        void draw_sleep_screen();
        void draw_idle_screen();
        void draw_wake_set_screen();
        void draw_time_set_screen();
        void draw_menu_screen();
        void draw_status_screen();
        void draw_brew_screen();

        void set_time(time_t time);
        bool synchronize_time(uint16_t timeout_secs);
        bool set_timezone(const char* timezone);
        void connect_to_wifi_or_fallback_to_ap(const char* ssid, const char* pass, uint32_t timeout_millis,
                                               const char* fallback_ap_name);
        bool is_wifi_connected() const;

        void save_persistent_data();

        void on_encoder_changed(int delta);
        void on_encoder_button_clicked();
        void on_encoder_button_double_clicked();
        void on_encoder_button_long_pressed();
        void on_any_input();

        void on_wifi_connected();
        void on_wifi_disconnected();

        bool has_water() const;
        bool has_pot() const;

        void set_boiler_state(bool on);

        void reset_brew_time();
        void set_brew_time(uint32_t secs);

        void reset_settings();

        void transition_to_state(state new_state);

        Stream& _log;

        int _persistent_data_addr = 0;
        persistent_data _data;

        Adafruit_SSD1306 _display;
        int64_t _last_encoder_count = 0;
        ESP32Encoder _encoder;
        OneButton _encoder_button;
        binary_switch _water_switch;
        binary_switch _pot_switch;
        int _boiler_ssr_pin = -1;
        uint32_t _last_input_time = 0;
        uint32_t _last_pot_time = 0;

        rotary_time_input _time_input;
        rotary_menu _menu;

        Timezone _timezone;

        bool _wifi_connected = false;
        bool _has_valid_time = false;

        config_web_server _config_web_server;

        state _state = state::idle;
    };
} // namespace mocca
