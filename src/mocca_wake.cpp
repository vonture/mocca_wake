#include "mocca_wake.hpp"

static constexpr uint8_t display_width = 128;
static constexpr uint8_t display_height = 64;
static constexpr uint8_t display_i2c_addr = 0x3C;

namespace mocca {
    mocca_wake::mocca_wake(Stream& log)
        : _log(log)
        , _display(display_width, display_height, &Wire)
        , _encoder() {}

    bool mocca_wake::init(int encoder_pin_a, int encoder_pin_b, int encoder_button_pin) {
        if (!_display.begin(SSD1306_SWITCHCAPVCC, display_i2c_addr)) {
            _log.println(F("SSD1306 allocation failed"));
            return false;
        }
        _display.setTextSize(1);
        _display.setTextColor(SSD1306_WHITE);

        _encoder.attachSingleEdge(encoder_pin_a, encoder_pin_b);

        return true;
    }

    void mocca_wake::step() {
        _display.clearDisplay();

        _display.setCursor(0, 0);             // Start at top-left corner

        _display.print(_encoder.getCount());

        _display.display();
    }
} // namespace mocca
