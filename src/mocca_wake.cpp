#include "mocca_wake.hpp"

#include <EEPROM.h>
#include <WiFi.h>
#include <functional>

namespace mocca {
    namespace {
        constexpr uint8_t display_width = 128;
        constexpr uint8_t display_height = 64;
        constexpr uint8_t display_i2c_addr = 0x3C;

        // constexpr const char* default_wifi_ssid = "Sprawl";
        // constexpr const char* default_wifi_password = "abc123ffff";
        constexpr const char* default_wifi_ssid = "CIK1000M_AC2.4G_5716";
        constexpr const char* default_wifi_password = "d0608c035ebc";
        constexpr const char* default_timezone = "America/Toronto";
        constexpr uint32_t default_wake_secs = 8 * SECS_PER_HOUR; // 8am

        constexpr uint32_t no_input_to_idle_millis = MILLIS_PER_SEC * 8;  // 8 sec
        constexpr uint32_t no_input_to_sleep_millis = MILLIS_PER_MIN * 5; // 5 mins

        constexpr uint32_t rotary_time_step = SECS_PER_MIN * 10; // 10 minutes per step

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

    bool mocca_wake::init(int encoder_pin_a, int encoder_pin_b, int encoder_button_pin, int persistent_data_addr) {
        if (!_display.begin(SSD1306_SWITCHCAPVCC, display_i2c_addr)) {
            _log.println("SSD1306 allocation failed");
            return false;
        }
        _display.setTextSize(1);
        _display.setTextColor(SSD1306_WHITE);

        _persistent_data_addr = persistent_data_addr;

        struct init_step {
            const char* name;
            std::function<bool(void)> function;
        };
        init_step init_steps[] = {
            {
                "Reading persistent data...",
                [&]() {
                    EEPROM.get(_persistent_data_addr, _data);
                    // if (!_data.crc_is_valid()) {
                    init_default_data(&_data);
                    save_persistent_data();
                    //}

                    // TODO: Move this into the state transition for idle->wake_set
                    _time_input.set_current_time(_data.last_wake_secs);
                    return true;
                },
            },
            {
                "Connecting to WiFi...",
                [&]() {
                    WiFi.begin(_data.wifi_ssid, _data.wifi_password);
                    _log.printf("WiFi connecting to %s (pass %s)...", _data.wifi_ssid, _data.wifi_password);
                    uint8_t wifi_status = WiFi.waitForConnectResult();
                    if (wifi_status != WL_CONNECTED) {
                        _log.printf(" failed. Status: %u\n", wifi_status);
                        return false;
                    }
                    _log.printf(" connected.\n");
                    return true;
                },
            },
            {
                "Synchronizing time...",
                [&]() {
                    _log.print("Waiting for time sync...");
                    if (!ezt::waitForSync()) {
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

                    return true;
                },
            },
            {
                "Initializing inputs...",
                [&]() {
                    _encoder.attachSingleEdge(encoder_pin_a, encoder_pin_b);

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
                    _encoder_button.attachDuringLongPress(
                        [](void* user_data) {
                            mocca_wake* wake = static_cast<mocca_wake*>(user_data);
                            wake->on_encoder_button_long_pressed();
                        },
                        this);

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
        _encoder_button.tick();

        int64_t encoder_count = _encoder.getCount();
        if (encoder_count != _last_encoder_count) {
            int delta = encoder_count - _last_encoder_count;
            on_encoder_changed(delta);
            _last_encoder_count = encoder_count;
        }

        uint32_t millis_since_last_input = millis() - _last_input_time;

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
        case state::brew:
            break;
        }
    }

    void mocca_wake::draw_init_screen(const char* step_name, size_t step_index, size_t step_count) {
        _display.clearDisplay();
        _display.setCursor(0, 0);
        _display.print(step_name);
        _display.display();
    }

    void mocca_wake::draw_notification_bar(const box& area, box* out_content_area) {
        time_t current_time = _timezone.now();
        const char* time_format_string = (current_time % 2 == 0) ? "g i" : "g:i";
        String current_time_text = _timezone.dateTime(current_time, time_format_string);
        _display.setTextSize(1);
        box text_area =
            draw_aligned_text(&_display, current_time_text.c_str(), text_alignment::right, text_alignment::top, area);

        *out_content_area = area;
        out_content_area->y += text_area.h;
        out_content_area->h -= text_area.h;
    }

    void mocca_wake::draw_sleep_screen() {
        _display.clearDisplay();
        _display.display();
    }

    void mocca_wake::draw_idle_screen() {
        _display.clearDisplay();

        String status_text;
        if (_data.current_wake > _timezone.now()) {
            status_text = _timezone.dateTime(_data.current_wake, "g:i A");
        } else {
            status_text = "Not set";
        }

        box full_screen_area(0, 0, _display.width(), _display.height());
        box content_area;
        draw_notification_bar(full_screen_area, &content_area);

        _display.setTextSize(2);
        draw_aligned_text(&_display, status_text.c_str(), text_alignment::center, text_alignment::center, content_area);

        _display.display();
    }

    void mocca_wake::draw_wake_set_screen() {
        _display.clearDisplay();

        box full_screen_area(0, 0, _display.width(), _display.height());
        box content_area;
        draw_notification_bar(full_screen_area, &content_area);

        _time_input.draw(&_display, content_area);
        _display.display();
    }

    void mocca_wake::save_persistent_data() {
        _data.update_crc();
        EEPROM.put(_persistent_data_addr, _data);
        EEPROM.commit();
    }

    void mocca_wake::on_encoder_changed(int delta) {
        _log.print("Encoder changed: ");
        _log.println(delta);

        switch (_state) {
        case state::sleep:
            transition_to_state(state::idle);
            break;
        case state::wake_set:
            _time_input.on_encoder_changed(delta);
            break;

        case state::menu:
            // TODO: scroll menu
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
            set_brew_time(_time_input.get_current_time());
            transition_to_state(state::idle);
            break;
        case state::menu:
            // TODO: handle menu action
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
        }

        on_any_input();
    }

    void mocca_wake::on_any_input() {
        _last_input_time = millis();
    }

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
            _time_input.set_current_time(_data.last_wake_secs);
            break;
        }
    }
} // namespace mocca
