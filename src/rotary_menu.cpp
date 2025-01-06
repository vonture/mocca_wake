#include "rotary_menu.hpp"

#include <ezTime.h>

namespace mocca {
    namespace {
        uint32_t advance_selected_item(uint32_t current, int delta, uint32_t count) {
            int new_value = static_cast<int>(current) + delta;
            while (new_value < 0) {
                new_value += count;
            }
            while (new_value >= count) {
                new_value -= count;
            }
            return new_value;
        }

        constexpr size_t text_size = 2;
        constexpr size_t character_width = 6 * text_size;
        constexpr size_t half_character_pad = character_width / 2;
    } // namespace

    void rotary_time_input::set_current_time(uint32_t seconds, const char* default_option) {
        _seconds = seconds;
        _default_option = default_option ? default_option : "";
        _on_default_option = default_option != nullptr;
    }

    bool rotary_time_input::is_on_default_option() const {
        return _on_default_option;
    }

    uint32_t rotary_time_input::get_current_time() const {
        return _seconds;
    }

    void rotary_time_input::set_time_step(uint32_t seconds) {
        _step = seconds;
        // TODO: Round current time to step?
    }

    void rotary_time_input::on_encoder_changed(int delta) {
        if (_on_default_option) {
            _on_default_option = false;
        } else {
            _seconds = advance_selected_item(_seconds, delta * _step, SECS_PER_DAY);
        }
    }

    void rotary_time_input::draw(Adafruit_SSD1306* display, const box& area) {

        String cur_option = _default_option;
        if (!_on_default_option) {
            time_t t = previousMidnight(ezt::now()) + _seconds;
            cur_option = ezt::dateTime(t, "g:i a");
        }

        display->setTextSize(2);
        box center_text_area =
            draw_aligned_text(display, cur_option.c_str(), text_alignment::right, text_alignment::center, area);

        box carat_area(area.x, area.y, center_text_area.x - half_character_pad - area.x, area.h);
        draw_aligned_text(display, ">", text_alignment::right, text_alignment::center, carat_area);
    }

    void rotary_menu::set_menu_options(std::vector<rotary_menu_option>&& options, uint32_t selected_option) {
        _menu_options = std::move(options);
        if (!_menu_options.empty()) {
            _selected_option = selected_option % _menu_options.size();
        } else {
            _selected_option = 0;
        }

        _max_option_length = 0;
        for (const rotary_menu_option& option : _menu_options) {
            _max_option_length = std::max(_max_option_length, option.display_text.length());
        }
    }

    void rotary_menu::on_encoder_changed(int delta) {
        if (_menu_options.empty()) {
            return;
        }

        _selected_option = advance_selected_item(_selected_option, delta, _menu_options.size());
    }

    void rotary_menu::on_encoder_clicked() {
        if (_menu_options.empty()) {
            return;
        }

        rotary_menu_callback callback = _menu_options[_selected_option].callback;
        if (callback) {
            callback();
        }
    }

    void rotary_menu::draw(Adafruit_SSD1306* display, const box& area) {
        if (_menu_options.empty()) {
            return;
        }

        const String& cur_option = _menu_options[_selected_option].display_text;
        size_t cur_option_padding = ((_max_option_length - cur_option.length()) * character_width) + half_character_pad;

        display->setTextSize(text_size);
        box offset_area(area.x, area.y, area.w - cur_option_padding, area.h);
        box center_text_area =
            draw_aligned_text(display, cur_option.c_str(), text_alignment::right, text_alignment::center, offset_area);

        box carat_area(area.x, area.y, center_text_area.x - half_character_pad - area.x, area.h);
        draw_aligned_text(display, ">", text_alignment::right, text_alignment::center, carat_area);

        if (_menu_options.size() >= 2) {
            uint16_t other_option_text_left = center_text_area.x + half_character_pad;
            box area_above(other_option_text_left, area.y, area.right() - other_option_text_left,
                           center_text_area.y - area.y);

            uint32_t prev_index = advance_selected_item(_selected_option, -1, _menu_options.size());
            const String& prev_option = _menu_options[prev_index].display_text;
            draw_aligned_text(display, prev_option.c_str(), text_alignment::left, text_alignment::bottom, area_above);

            uint32_t next_index = advance_selected_item(_selected_option, 1, _menu_options.size());
            const String& next_option = _menu_options[next_index].display_text;
            box area_below(other_option_text_left, center_text_area.bottom(), area.right() - other_option_text_left,
                           area.bottom() - center_text_area.bottom());
            draw_aligned_text(display, next_option.c_str(), text_alignment::left, text_alignment::bottom, area_below);
        }
    }
} // namespace mocca
