#pragma once

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include <vector>

namespace mocca {
    class config_web_server {
      public:
        config_web_server();

        void init();
        void step();

      private:
        AsyncWebServer _server;

        struct wifi_network_info {
            String ssid;
            wifi_auth_mode_t encryption;
            int32_t rssi;
            String bssid;
            int32_t channel;

            void set_from_network_item(uint8_t network_item);
            JsonDocument to_json() const;
        };
        std::vector<wifi_network_info> _networks;
        uint32_t _last_scan_time = 0;
    };
} // namespace mocca
