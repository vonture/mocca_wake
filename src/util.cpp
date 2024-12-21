#include "util.hpp"

namespace mocca {
    box::box()
        : x(0)
        , y(0)
        , w(0)
        , h(0) {}

    box::box(int32_t x, int32_t y, int32_t w, int32_t h)
        : x(x)
        , y(y)
        , w(w)
        , h(h) {}

    box draw_aligned_text(Adafruit_SSD1306* display, const char* text, text_alignment horizontal_alignment,
                          text_alignment vertical_alignment, const box& area) {

        int16_t text_x, text_y;
        uint16_t text_w, text_h;
        display->getTextBounds(text, 0, 0, &text_x, &text_y, &text_w, &text_h);

        auto align_axis = [](int area_begin, int area_size, int text_size, text_alignment alignment) {
            switch (alignment) {
            case text_alignment::left:
                return area_begin;
            case text_alignment::right:
                return area_begin + area_size - text_size;
            case text_alignment::center:
                return area_begin + (area_size / 2) - (text_size / 2);
            default:
                return 0;
            }
        };

        int16_t cursor_x = align_axis(area.x, area.w, text_w, horizontal_alignment);
        int16_t cursor_y = align_axis(area.y, area.h, text_h, vertical_alignment);
        display->setCursor(cursor_x, cursor_y);
        display->print(text);

        box print_area;
        print_area.x = cursor_x - text_x;
        print_area.y = cursor_y - text_y;
        print_area.w = text_w;
        print_area.h = text_h;

        return print_area;
    }

    binary_switch::binary_switch() {}

    void binary_switch::init(uint8_t pin, uint8_t mode, bool active_low) {
        _pin = pin;
        pinMode(_pin, mode);
        _pressed_state = active_low ? LOW : HIGH;
    }

    bool binary_switch::get_state() const {
        return digitalRead(_pin) == _pressed_state;
    }
} // namespace mocca
