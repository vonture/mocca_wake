#pragma once

#include <Adafruit_SSD1306.h>
#include <ezTime.h>

namespace mocca {
#define MILLIS_PER_SEC (1000)
#define MILLIS_PER_MIN (MILLIS_PER_SEC * SECS_PER_MIN)

    struct point2d {
        int32_t x = 0;
        int32_t y = 0;
    };

    struct size2d {
        int32_t w = 0;
        int32_t h = 0;
    };

    struct box {
        union {
            struct {
                int32_t x, y;
            };
            point2d pos;
        };
        union {
            struct {
                int32_t w, h;
            };
            size2d size;
        };

        box();
        box(int32_t x, int32_t y, int32_t w, int32_t h);

        int32_t right() const;
        int32_t bottom() const;
    };

    enum class text_alignment {
        left,
        top = left,
        right,
        bottom = right,
        center,
    };

    box draw_aligned_text(Adafruit_SSD1306* display, const char* text, text_alignment horizontal_alignment,
                          text_alignment vertical_alignment, const box& area);

    class binary_switch {
      public:
        binary_switch();

        void init(uint8_t pin, uint8_t mode, bool active_low);

        bool get_state() const;

      private:
        int _pin = -1;
        int _pressed_state = 0;
    };
}; // namespace mocca
