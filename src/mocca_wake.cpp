#include "mocca_wake.hpp"

#include <EEPROM.h>
#include <WiFi.h>
#include <functional>

namespace mocca {
    namespace {
        constexpr uint8_t display_width = 128;
        constexpr uint8_t display_height = 64;
        constexpr uint8_t display_i2c_addr = 0x3C;

        constexpr const char* default_wifi_ssid = "langli";
        constexpr const char* default_wifi_password = "abc123ffff";
        constexpr const char* default_timezone = "America/Toronto";
        constexpr uint32_t default_wake_secs = 8 * SECS_PER_HOUR; // 8am

        constexpr const char* config_ap_ssid = "MoccaWake";

        constexpr const char* spash_screen_text = "MoccaWake";

        constexpr uint32_t no_input_to_idle_millis = MILLIS_PER_SEC * 8;  // 8 sec
        constexpr uint32_t no_input_to_sleep_millis = MILLIS_PER_MIN * 5; // 5 mins

        constexpr uint32_t wifi_wait_millis = MILLIS_PER_SEC * 5; // 5 sec
        constexpr uint32_t time_sync_wait_secs = 5;               // 5 sec

        constexpr uint32_t rotary_time_step = SECS_PER_MIN * 10; // 10 minutes per step
        constexpr uint32_t rotary_count_divisor = 2;             // For half-quad mode

        constexpr uint32_t stop_brew_with_no_pot_millis =
            MILLIS_PER_MIN * 10; // If no pot is present while brewing for 10 minutes, stop the brew

        // 8x8 notification bar icons
        constexpr unsigned char wifi_icon[] = {0x00, 0x3c, 0x42, 0x81, 0x3c, 0x42, 0x18, 0x18};
        constexpr unsigned char pot_icon[] = {0x38, 0xff, 0x7d, 0x7d, 0x7d, 0x7f, 0x7c, 0x7c};
        constexpr unsigned char water_icon[] = {0x10, 0x38, 0x38, 0x7c, 0x7c, 0xfe, 0x7c, 0x38};

        void init_default_data(persistent_data* data) {
            memset(data, 0, sizeof(persistent_data));
            strcpy(data->wifi_ssid, default_wifi_ssid);
            strcpy(data->wifi_password, default_wifi_password);
            strcpy(data->timezone, default_timezone);
            data->last_wake_secs = default_wake_secs;
        }

        const char* state_name(state s) {
            switch (s) {
            case state::sleep:
                return "sleep";
            case state::idle:
                return "idle";
            case state::wake_set:
                return "wake_set";
            case state::brew:
                return "brew";
            default:
                return "unknown";
            }
        }
    } // namespace

    mocca_wake::mocca_wake(Stream& log)
        : _log(log)
        , _display(display_width, display_height, &Wire)
        , _encoder() {
        _time_input.set_time_step(rotary_time_step);
    }

    bool mocca_wake::init(int encoder_pin_a, int encoder_pin_b, int encoder_button_pin, int water_switch_pin,
                          int pot_switch_pin, int boiler_ssr_pin, int persistent_data_addr) {
        if (!_display.begin(SSD1306_SWITCHCAPVCC, display_i2c_addr)) {
            _log.println("SSD1306 allocation failed");
            return false;
        }
        _display.setTextColor(SSD1306_WHITE);

        _persistent_data_addr = persistent_data_addr;
        _boiler_ssr_pin = boiler_ssr_pin;

        struct init_step {
            const char* name;
            std::function<bool(void)> function;
        };
        init_step init_steps[] = {
            {
                "Initializing peripherals...",
                [&]() {
                    _encoder.attachHalfQuad(encoder_pin_a, encoder_pin_b);

                    _encoder_button.setup(encoder_button_pin, INPUT_PULLUP, true);
                    _encoder_button.setClickMs(200);
                    _encoder_button.attachClick(
                        [](void* user_data) {
                            mocca_wake* wake = static_cast<mocca_wake*>(user_data);
                            wake->on_encoder_button_clicked();
                        },
                        this);
                    _encoder_button.attachDoubleClick(
                        [](void* user_data) {
                            mocca_wake* wake = static_cast<mocca_wake*>(user_data);
                            wake->on_encoder_button_double_clicked();
                        },
                        this);
                    _encoder_button.attachLongPressStart(
                        [](void* user_data) {
                            mocca_wake* wake = static_cast<mocca_wake*>(user_data);
                            wake->on_encoder_button_long_pressed();
                        },
                        this);

                    _water_switch.init(water_switch_pin, INPUT_PULLUP, true);
                    _pot_switch.init(pot_switch_pin, INPUT_PULLUP, true);

                    pinMode(_boiler_ssr_pin, OUTPUT);
                    set_boiler_state(false);

                    return true;
                },
            },
            {
                "Reading persistent data...",
                [&]() {
                    EEPROM.get(_persistent_data_addr, _data);
                    // if (!_data.crc_is_valid()) {
                    _log.println("Persistant data CRC missmatch. Resetting to default.");
                    init_default_data(&_data);
                    save_persistent_data();
                    //}

                    return true;
                },
            },
            {
                "Connecting to WiFi...",
                [&]() {
                    connect_to_wifi_or_fallback_to_ap(_data.wifi_ssid, _data.wifi_password, wifi_wait_millis,
                                                      config_ap_ssid);
                    return true;
                },
            },
            {
                "Synchronizing time...",
                [&]() {
                    if (is_wifi_connected()) {
                        synchronize_time(time_sync_wait_secs);
                    }
                    return true;
                },
            },
            {
                "Start web server...",
                [&]() {
                    _config_web_server.init();
                    return true;
                },
            },
        };

        constexpr size_t init_step_count = sizeof(init_steps) / sizeof(*init_steps);
        for (size_t step_idx = 0; step_idx < init_step_count; step_idx++) {
            const init_step& step = init_steps[step_idx];
            draw_init_screen(step.name, step_idx, init_step_count);
            if (!step.function()) {
                return false;
            }
        }

        return true;
    }

    void mocca_wake::step() {
        bool boiler_should_be_on = false;

        _config_web_server.step();

        _encoder_button.tick();

        if (_wifi_connected != (WiFi.status() == WL_CONNECTED)) {
            _wifi_connected = !_wifi_connected;
            if (_wifi_connected) {
                on_wifi_connected();
            } else {
                on_wifi_disconnected();
            }
        }

        int64_t encoder_count = _encoder.getCount() / rotary_count_divisor;
        if (encoder_count != _last_encoder_count) {
            int delta = encoder_count - _last_encoder_count;
            on_encoder_changed(delta);
            _last_encoder_count = encoder_count;
        }

        uint32_t millis_since_last_input = millis() - _last_input_time;

        if (has_pot()) {
            _last_pot_time = millis();
        }
        uint32_t millis_since_pot = millis() - _last_pot_time;

        switch (_state) {
        case state::sleep:
            break;
        case state::idle:
            if (millis_since_last_input >= no_input_to_sleep_millis) {
                transition_to_state(state::sleep);
                break;
            }
            draw_idle_screen();
            break;
        case state::wake_set:
            if (millis_since_last_input >= no_input_to_idle_millis) {
                transition_to_state(state::idle);
                break;
            }
            draw_wake_set_screen();
            break;
        case state::time_set:
            if (millis_since_last_input >= no_input_to_idle_millis) {
                transition_to_state(state::idle);
                break;
            }
            draw_time_set_screen();
            break;
        case state::menu:
            if (millis_since_last_input >= no_input_to_idle_millis) {
                transition_to_state(state::idle);
                break;
            }
            draw_menu_screen();
            break;
        case state::status:
            if (millis_since_last_input >= no_input_to_idle_millis) {
                transition_to_state(state::idle);
                break;
            }
            draw_status_screen();
            break;
        case state::brew:
            if (!has_water()) {
                transition_to_state(state::idle);
                break;
            }

            if (!has_pot() && millis_since_pot >= stop_brew_with_no_pot_millis) {
                transition_to_state(state::idle);
                break;
            }

            if (has_pot()) {
                boiler_should_be_on = true;
            }

            draw_brew_screen();
            break;
        }

        set_boiler_state(boiler_should_be_on);
    }

    void mocca_wake::draw_init_screen(const char* step_name, size_t step_index, size_t step_count) {
        _display.clearDisplay();

        box full_screen_area(0, 0, _display.width(), _display.height());

        box content_area;
        draw_notification_bar(full_screen_area, &content_area);

        _display.setTextSize(1);
        draw_aligned_text(&_display, step_name, text_alignment::left, text_alignment::bottom, content_area);

        _display.setTextSize(2);
        draw_aligned_text(&_display, spash_screen_text, text_alignment::center, text_alignment::center, content_area);

        _display.display();
    }

    void mocca_wake::draw_notification_bar(const box& area, box* out_content_area) {
        auto draw_notification_icon = [](Adafruit_GFX* display, int16_t* cursor, const uint8_t bitmap8x8[]) {
            display->drawBitmap(*cursor, 0, bitmap8x8, 8, 8, SSD1306_WHITE, SSD1306_BLACK);
            *cursor += 10;
        };

        int16_t notification_icon_cursor = 0;
        if (WiFi.status() == WL_CONNECTED) {
            draw_notification_icon(&_display, &notification_icon_cursor, wifi_icon);
        }
        if (has_water()) {
            draw_notification_icon(&_display, &notification_icon_cursor, water_icon);
        }
        if (has_pot()) {
            draw_notification_icon(&_display, &notification_icon_cursor, pot_icon);
        }

        String current_time_text;
        if (_has_valid_time) {
            time_t current_time = _timezone.now();
            const char* time_format_string = (current_time % 2 == 0) ? "g i a" : "g:i a";
            current_time_text = _timezone.dateTime(current_time, time_format_string);
        } else {
            current_time_text = (ezt::now() % 2 != 0) ? "0:00" : " ";
        }

        _display.setTextSize(1);
        box text_area =
            draw_aligned_text(&_display, current_time_text.c_str(), text_alignment::right, text_alignment::top, area);

        *out_content_area = area;
        out_content_area->y += text_area.h;
        out_content_area->h -= text_area.h;
    }

    void mocca_wake::draw_time_to_idle_bar() {
        constexpr uint32_t idle_timeout = no_input_to_idle_millis;
        constexpr uint32_t max_idle_bar_duration =
            MILLIS_PER_SEC * 1; // Start drawing the idle timeout bar 2 seconds before idleing.

        uint32_t idle_bar_start = std::min(max_idle_bar_duration, idle_timeout);
        uint32_t millis_since_last_input = millis() - _last_input_time;
        uint32_t time_to_idle = idle_timeout - millis_since_last_input;
        if (time_to_idle > idle_bar_start) {
            return;
        }

        uint32_t bar_length = (_display.width() * time_to_idle) / idle_bar_start;
        _display.drawLine(0, _display.height() - 1, bar_length, _display.height() - 1, SSD1306_WHITE);
    }

    void mocca_wake::draw_sleep_screen() {
        _display.clearDisplay();
        _display.display();
    }

    void mocca_wake::draw_idle_screen() {
        _display.clearDisplay();

        box full_screen_area(0, 0, _display.width(), _display.height());
        box content_area;
        draw_notification_bar(full_screen_area, &content_area);

        if (_data.current_wake > _timezone.now()) {
            String time_text = _timezone.dateTime(_data.current_wake, "g:i a");
            _display.setTextSize(2);
            draw_aligned_text(&_display, ">", text_alignment::left, text_alignment::center, content_area);
            draw_aligned_text(&_display, time_text.c_str(), text_alignment::center, text_alignment::center,
                              content_area);
            draw_aligned_text(&_display, "<", text_alignment::right, text_alignment::center, content_area);
        } else {
            _display.setTextSize(2);
            draw_aligned_text(&_display, "Ready", text_alignment::center, text_alignment::center, content_area);
        }

        _display.setTextSize(1);
        draw_aligned_text(&_display, "Press to brew", text_alignment::left, text_alignment::bottom, content_area);

        _display.display();
    }

    void mocca_wake::draw_wake_set_screen() {
        _display.clearDisplay();

        box full_screen_area(0, 0, _display.width(), _display.height());
        box content_area;
        draw_notification_bar(full_screen_area, &content_area);

        _time_input.draw(&_display, content_area);

        if (_time_input.is_on_default_option()) {
            _display.setTextSize(1);
            draw_aligned_text(&_display, "Scroll to change time", text_alignment::left, text_alignment::bottom,
                              content_area);
        }

        draw_time_to_idle_bar();

        _display.display();
    }

    void mocca_wake::draw_time_set_screen() {
        _display.clearDisplay();

        box full_screen_area(0, 0, _display.width(), _display.height());
        box content_area;
        draw_notification_bar(full_screen_area, &content_area);

        _time_input.draw(&_display, content_area);

        draw_time_to_idle_bar();

        _display.display();
    }

    void mocca_wake::draw_menu_screen() {
        _display.clearDisplay();

        box full_screen_area(0, 0, _display.width(), _display.height());
        box content_area;
        draw_notification_bar(full_screen_area, &content_area);

        _menu.draw(&_display, content_area);

        draw_time_to_idle_bar();

        _display.display();
    }

    void mocca_wake::draw_status_screen() {
        _display.clearDisplay();

        box full_screen_area(0, 0, _display.width(), _display.height());
        box content_area;
        draw_notification_bar(full_screen_area, &content_area);

        String status_text;
        status_text += "Network: " + WiFi.SSID() + "\n";
        status_text += "IP: " + WiFi.localIP().toString() + "\n";
        status_text += "Timezone: " + _timezone.getTimezoneName() + "\n";

        _display.setTextSize(1);
        draw_aligned_text(&_display, status_text.c_str(), text_alignment::left, text_alignment::center, content_area);

        _display.display();
    }

    void mocca_wake::draw_brew_screen() {
        _display.clearDisplay();

        box full_screen_area(0, 0, _display.width(), _display.height());
        box content_area;
        draw_notification_bar(full_screen_area, &content_area);

        _display.setTextSize(2);
        const char* display_text = has_pot() ? "Brewing" : "Paused";
        draw_aligned_text(&_display, display_text, text_alignment::center, text_alignment::center, content_area);

        _display.setTextSize(1);
        draw_aligned_text(&_display, "Long press to cancel", text_alignment::left, text_alignment::bottom,
                          content_area);

        _display.display();
    }

    void mocca_wake::set_time(time_t time) {
        _timezone.setTime(time);
        _has_valid_time = true;
    }

    bool mocca_wake::synchronize_time(uint16_t timeout_secs) {
        _has_valid_time = false;
        _log.print("Waiting for time sync...");
        if (!ezt::waitForSync(time_sync_wait_secs)) {
            _log.println(" failed.");
            return false;
        }
        _log.println(" done.");

        _log.printf("Setting timezone to %s...", _data.timezone);
        if (!_timezone.setLocation(_data.timezone)) {
            _log.println(" failed.");
            return false;
        }
        _log.printf(" done. Set timezone is %s.\n", _timezone.getTimezoneName().c_str());
        _has_valid_time = true;
        return true;
    }

    void mocca_wake::connect_to_wifi_or_fallback_to_ap(const char* ssid, const char* pass, uint32_t timeout_millis,
                                                       const char* fallback_ap_name) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, pass);
        _log.printf("WiFi connecting to \"%s\" (pass \"%s\")...", _data.wifi_ssid, _data.wifi_password);
        uint8_t wifi_status = WiFi.waitForConnectResult(timeout_millis);
        _wifi_connected = (wifi_status == WL_CONNECTED);
        _log.printf("%s.\n", _wifi_connected ? "connected" : "failed");
        if (_wifi_connected) {
            if (strcmp(_data.wifi_ssid, ssid) != 0 || strcmp(_data.wifi_password, pass)) {
                strcpy(_data.wifi_ssid, ssid);
                strcpy(_data.wifi_password, pass);
                save_persistent_data();
            }

            return;
        }

        _log.printf("Starting WiFi access point \"%s\".", fallback_ap_name);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(fallback_ap_name);
    }

    bool mocca_wake::is_wifi_connected() const {
        return _wifi_connected;
    }

    void mocca_wake::save_persistent_data() {
        _log.println("Saving persistent data.");
        _data.update_crc();
        EEPROM.put(_persistent_data_addr, _data);
        EEPROM.commit();
    }

    void mocca_wake::on_encoder_changed(int delta) {
        //_log.printf("Encoder changed: %d\n", delta);

        switch (_state) {
        case state::sleep:
            transition_to_state(state::idle);
            break;
        case state::wake_set:
            _time_input.on_encoder_changed(delta);
            break;
        case state::time_set:
            _time_input.on_encoder_changed(delta);
            break;
        case state::menu:
            _menu.on_encoder_changed(delta);
            break;
        }

        on_any_input();
    }

    void mocca_wake::on_encoder_button_clicked() {
        _log.println("Encoder button clicked.");

        switch (_state) {
        case state::sleep:
        case state::idle:
            transition_to_state(state::wake_set);
            break;
        case state::wake_set:
            if (_time_input.is_on_default_option()) {
                transition_to_state(state::brew);
            } else {
                set_brew_time(_time_input.get_current_time());
                transition_to_state(state::idle);
            }
            break;
        case state::time_set:
            set_time(previousMidnight(_timezone.now()) + _time_input.get_current_time());
            transition_to_state(state::idle);
            break;
        case state::menu:
            _menu.on_encoder_clicked();
            break;
        }

        on_any_input();
    }

    void mocca_wake::on_encoder_button_double_clicked() {
        _log.println("Encoder button double clicked.");
        on_any_input();
    }

    void mocca_wake::on_encoder_button_long_pressed() {
        _log.println("Encoder button long pressed.");

        switch (_state) {
        case state::sleep:
        case state::idle:
            transition_to_state(state::menu);
            break;
        case state::brew:
            transition_to_state(state::idle);
            break;
        }

        on_any_input();
    }

    void mocca_wake::on_any_input() {
        _last_input_time = millis();
    }

    void mocca_wake::on_wifi_connected() {
        synchronize_time(time_sync_wait_secs);
    }

    void mocca_wake::on_wifi_disconnected() {}

    bool mocca_wake::has_water() const {
        return _water_switch.get_state();
    }

    bool mocca_wake::has_pot() const {
        return _pot_switch.get_state();
    }

    void mocca_wake::set_boiler_state(bool on) {
        digitalWrite(_boiler_ssr_pin, on ? HIGH : LOW);
    }

    void mocca_wake::reset_brew_time() {}

    void mocca_wake::set_brew_time(uint32_t secs) {
        // Find the next time_t that this "secs" will happen
        time_t now = _timezone.now();
        time_t t = previousMidnight(now) + secs;
        if (t < now) {
            t += SECS_PER_DAY;
        }

        _log.print("Setting brew time to: ");
        _log.println(_timezone.dateTime(t));

        _data.current_wake = t;
        _data.last_wake_secs = secs;
        save_persistent_data();

        // TODO: Make the wake up event
    }

    void mocca_wake::reset_settings() {}

    void mocca_wake::transition_to_state(state new_state) {
        _log.printf("Transition %s to %s\n", state_name(_state), state_name(new_state));
        _state = new_state;

        // Reset the last input time so that we don't transition multiple states too quickly.
        _last_input_time = millis();

        switch (_state) {
        case state::sleep:
            // Clear the display only on transition into slep, not every tick.
            draw_sleep_screen();
            break;

        case state::wake_set:
            _time_input.set_current_time(_data.last_wake_secs, "Brew now");
            break;

        case state::time_set: {
            time_t now = _timezone.now();
            _time_input.set_current_time(now - previousMidnight(now), nullptr);
        } break;

        case state::menu: {
            std::vector<rotary_menu_option> options;
            if (_data.current_wake > _timezone.now()) {
                options.push_back({
                    "Clear brew",
                    [this]() {
                        reset_brew_time();
                        transition_to_state(state::idle);
                    },
                });
            }
            options.push_back({"Reset", [this]() {
                                   reset_settings();
                                   transition_to_state(state::idle);
                               }});
            options.push_back({
                "Set time",
                [this]() { transition_to_state(state::time_set); },
            });
            options.push_back({
                "Status",
                [this]() { transition_to_state(state::status); },
            });
            _menu.set_menu_options(std::move(options), 0);
        } break;

        case state::brew:
            break;
        }
    }
} // namespace mocca
