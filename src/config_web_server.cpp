#include "config_web_server.hpp"

#include "util.hpp"

#include <AsyncJson.h>

namespace mocca {

    constexpr uint16_t web_server_port = 80;
    constexpr uint32_t network_refresh_after_request_time = MILLIS_PER_SEC * 60;

    constexpr const char html_content[] PROGMEM = R"(
<!doctype html>
<html>
<body>
    <div id="networks" />
    <script>
        async function refreshNetworksList() {
            const url = "wifi_networks";
            const response = await fetch(url);
            if (!response.ok) {
            throw new Error(`Response status: ${response.status}`);
            }

            const json = await response.json();
            let networkDivs = [];
            json.networks.forEach(function(network) {
                let networkDiv = document.createElement("div");
                networkDiv.appendChild(document.createTextNode(network.ssid));
                networkDivs.push(networkDiv);

            });

            let networksDiv = document.getElementById("networks");
            networksDiv.replaceChildren(...networkDivs);

            setTimeout(function() {
                refreshNetworksList();
            }, 1000);
        }

        refreshNetworksList();
    </script>
</body>
</html>
)";

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
        : _server(web_server_port) {

        _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
            on_any_web_request();
            request->send(200, "text/html", html_content);
        });

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
    }

    void config_web_server::init() {
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