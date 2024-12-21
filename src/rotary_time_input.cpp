#include "rotary_time_input.hpp"

#include <ezTime.h>

namespace mocca {
    void rotary_time_input::set_current_time(uint32_t seconds) {
        _seconds = seconds;
    }

    uint32_t rotary_time_input::get_current_time() const {
        return _seconds;
    }

    void rotary_time_input::set_time_step(uint32_t seconds) {
        _step = seconds;
        // TODO: Round current time to step?
    }

    void rotary_time_input::on_encoder_changed(int delta) {
        int delta_seconds = delta * _step;
        int new_seconds = static_cast<int>(_seconds) + (delta * _step);
        while (new_seconds < 0) {
            new_seconds += SECS_PER_DAY;
        }
        while (new_seconds >= SECS_PER_DAY) {
            new_seconds -= SECS_PER_DAY;
        }
        _seconds = new_seconds;
    }

    void rotary_time_input::draw(Adafruit_SSD1306* display, const box& area) {
        time_t t = previousMidnight(ezt::now()) + _seconds;
        String time_text = ezt::dateTime(t, "~> g:i a");

        display->setTextSize(2);
        draw_aligned_text(display, time_text.c_str(), text_alignment::right, text_alignment::center, area);
    }

} // namespace mocca
