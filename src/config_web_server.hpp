#pragma once

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>

#include <vector>

namespace mocca {
    using wifi_connect_callback = std::function<void(const char* ssid, const char* password)>;
    using timezone_set_callback = std::function<bool(const char* timezone)>;

    class config_web_server {
      public:
        config_web_server();

        void init();
        void step();

        void set_wifi_callback(wifi_connect_callback callback);
        void set_timezone_callback(timezone_set_callback callback);

      private:
        void on_any_web_request();

        AsyncWebServer _server;
        AsyncCallbackJsonWebHandler _wifi_connect_handler;
        AsyncCallbackJsonWebHandler _set_timezone_handler;

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
        uint32_t _last_request = 0;

        wifi_connect_callback _wifi_set_callback;
        timezone_set_callback _timezone_set_callback;

        std::vector<std::function<void(void)>> _main_thread_tasks;
    };
} // namespace mocca
