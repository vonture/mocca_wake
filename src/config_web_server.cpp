#include "config_web_server.hpp"

#include "util.hpp"

#include <LittleFS.h>

namespace mocca {

    constexpr uint16_t web_server_port = 80;
    constexpr uint32_t network_refresh_after_request_time = MILLIS_PER_SEC * 60;

    static const char* auth_mode_name(wifi_auth_mode_t auth_mode) {
        switch (auth_mode) {
        case WIFI_AUTH_OPEN:
            return "open";
        case WIFI_AUTH_WEP:
            return "wep";
        case WIFI_AUTH_WPA_PSK:
        case WIFI_AUTH_WPA2_PSK:
        case WIFI_AUTH_WPA_WPA2_PSK:
        case WIFI_AUTH_WPA2_ENTERPRISE:
        case WIFI_AUTH_WPA3_PSK:
        case WIFI_AUTH_WPA2_WPA3_PSK:
        case WIFI_AUTH_WAPI_PSK:
        case WIFI_AUTH_WPA3_ENT_192:
            return "wpa";
        default:
            return "unknown";
        }
    };

    config_web_server::config_web_server()
        : _server(web_server_port)
        , _wifi_connect_handler("/wifi_connect")
        , _set_timezone_handler("/set_timezone") {

        _server.on("/wifi_networks", HTTP_GET, [this](AsyncWebServerRequest* request) {
            on_any_web_request();

            AsyncJsonResponse* response = new AsyncJsonResponse();
            JsonObject root = response->getRoot().to<JsonObject>();

            JsonArray networks_json = root["networks"].to<JsonArray>();
            for (const wifi_network_info& network : _networks) {
                networks_json.add(network.to_json());
            }

            response->setLength();
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
        });

        auto simple_json_response = [](AsyncWebServerRequest* request, int code, const char* failure_reason) {
            AsyncJsonResponse* response = new AsyncJsonResponse();
            JsonObject response_root = response->getRoot().to<JsonObject>();
            if (failure_reason) {
                response_root["status"] = "failed";
                response_root["reason"] = failure_reason;
            } else {
                response_root["status"] = "ok";
            }
            response->setLength();
            response->addHeader("Access-Control-Allow-Origin", "*");
            response->setCode(code);
            request->send(response);
        };

        _wifi_connect_handler.setMethod(HTTP_POST);
        _wifi_connect_handler.onRequest(
            [this, simple_json_response](AsyncWebServerRequest* request, JsonVariant& json) {
                JsonString ssid = json["ssid"];
                JsonString password = json["pass"];
                if (ssid.isNull() || password.isNull()) {
                    simple_json_response(request, 400, "null \"ssid\" or \"pass\" fields");
                    return;
                }

                String ssid_copy(ssid.c_str());
                String password_copy(password.c_str());
                _main_thread_tasks.push_back([this, ssid_copy, password_copy]() {
                    if (_wifi_set_callback) {
                        _wifi_set_callback(ssid_copy.c_str(), password_copy.c_str());
                    }
                });

                simple_json_response(request, 200, nullptr);
            });
        _server.addHandler(&_wifi_connect_handler);

        _set_timezone_handler.setMethod(HTTP_POST);
        _set_timezone_handler.onRequest(
            [this, simple_json_response](AsyncWebServerRequest* request, JsonVariant& json) {
                JsonString timezone = json["timezone"];
                if (timezone.isNull()) {
                    simple_json_response(request, 400, "null \"timezone\" field");
                    return;
                }

                String timezone_copy(timezone.c_str());
                _main_thread_tasks.push_back([this, timezone_copy]() {
                    if (_timezone_set_callback) {
                        _timezone_set_callback(timezone_copy.c_str());
                    }
                });

                simple_json_response(request, 200, nullptr);
            });
        _server.addHandler(&_set_timezone_handler);

        _server.serveStatic("/", LittleFS, "/www/", "max-age=600");
    }

    void config_web_server::init() {
        LittleFS.begin(true);

        _server.begin();
    }

    void config_web_server::step() {
        uint32_t cur_time = millis();
        if (cur_time < _last_request + network_refresh_after_request_time && WiFi.scanComplete() == WIFI_SCAN_FAILED) {
            WiFi.scanNetworks(true);
        }

        int16_t scan_result = WiFi.scanComplete();
        if (scan_result >= 0) {
            _networks.resize(scan_result);
            for (int16_t network_idx = 0; network_idx < scan_result; network_idx++) {
                _networks[network_idx].set_from_network_item(network_idx);
            }
            WiFi.scanDelete();
        }

        for (auto main_thread_task : _main_thread_tasks) {
            main_thread_task();
        }
        _main_thread_tasks.clear();
    }

    void config_web_server::set_wifi_callback(wifi_connect_callback callback) {
        _wifi_set_callback = callback;
    }

    void config_web_server::set_timezone_callback(timezone_set_callback callback) {
        _timezone_set_callback = callback;
    }

    void config_web_server::on_any_web_request() {
        _last_request = millis();
    }

    void config_web_server::wifi_network_info::set_from_network_item(uint8_t network_item) {
        ssid = WiFi.SSID(network_item);
        encryption = WiFi.encryptionType(network_item);
        rssi = WiFi.RSSI(network_item);
        bssid = WiFi.BSSIDstr(network_item);
        channel = WiFi.channel(network_item);
    }

    JsonDocument config_web_server::wifi_network_info::to_json() const {
        JsonDocument json;
        json["ssid"] = ssid;
        json["encryption"] = auth_mode_name(encryption);
        json["bssid"] = bssid;
        json["rssi"] = rssi;
        json["channel"] = channel;
        return json;
    }
} // namespace mocca