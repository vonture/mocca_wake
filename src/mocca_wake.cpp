#include "mocca_wake.hpp"

#include <EEPROM.h>
#include <WiFi.h>
#include <ezTime.h>
#include <functional>

static constexpr uint8_t display_width = 128;
static constexpr uint8_t display_height = 64;
static constexpr uint8_t display_i2c_addr = 0x3C;

// static constexpr const char* default_wifi_ssid = "Sprawl";
// static constexpr const char* default_wifi_password = "abc123ffff";
static constexpr const char* default_wifi_ssid = "CIK1000M_AC2.4G_5716";
static constexpr const char* default_wifi_password = "d0608c035ebc";
static constexpr const char* default_timezone = "America/Toronto";
static constexpr uint32_t default_wake_secs = 8 * SECS_PER_HOUR; // 8am

namespace mocca {
    namespace {
        void init_default_data(persistent_data* data) {
            memset(data, 0, sizeof(persistent_data));
            strcpy(data->wifi_ssid, default_wifi_ssid);
            strcpy(data->wifi_password, default_wifi_password);
            strcpy(data->timezone, default_timezone);
            data->last_wake_secs = default_wake_secs;
        }
    } // namespace

    mocca_wake::mocca_wake(Stream& log)
        : _log(log)
        , _display(display_width, display_height, &Wire)
        , _encoder() {}

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
                "Initializing encoder...",
                [&]() {
                    _encoder.attachSingleEdge(encoder_pin_a, encoder_pin_b);
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
        _display.clearDisplay();

        _display.setCursor(0, 0); // Start at top-left corner
        _display.print("step");
        //_display.print(_encoder.getCount());

        _display.display();
    }

    void mocca_wake::draw_init_screen(const char* step_name, size_t step_index, size_t step_count) {
        _display.clearDisplay();
        _display.setCursor(0, 0);
        _display.print(step_name);
        _display.display();
    }

    void mocca_wake::save_persistent_data() {
        _data.update_crc();
        EEPROM.put(_persistent_data_addr, _data);
        EEPROM.commit();
    }
} // namespace mocca
