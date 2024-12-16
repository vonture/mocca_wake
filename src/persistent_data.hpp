#pragma once

#include <Arduino.h>
#include <ezTime.h>

namespace mocca {
    struct persistent_data {
        uint32_t crc = 0;
        char wifi_ssid[32] = {0};
        char wifi_password[63] = {0};
        char timezone[64] = {0};
        time_t current_wake = 0;
        uint32_t last_wake_secs = 0; // Seconds into the day that the last wake was set.

        bool crc_is_valid() const;
        void update_crc();
    };
} // namespace mocca