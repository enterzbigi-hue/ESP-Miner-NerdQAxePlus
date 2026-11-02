#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include "hash_capture.h"
#include "handler_tuner.h"
#include "auto_tuner.h"
#include "psram_allocator.h"
#include "http_cors.h"
#include "http_utils.h"

esp_err_t GET_hash_capture(httpd_req_t *req) {
    ConGuard guard(http_server, req);
    if (is_network_allowed(req) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    if (set_cors_headers(req) != ESP_OK) return ESP_FAIL;
    uint32_t after = 0;
    if (httpd_req_get_url_query_len(req)) {
        char query[48], value[16];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
            httpd_query_key_value(query, "after", value, sizeof(value)) != ESP_OK ||
            !value[0] || strspn(value, "0123456789") != strlen(value))
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid sequence");
        errno = 0;
        unsigned long long parsed = strtoull(value, nullptr, 10);
        if (errno || parsed > UINT32_MAX)
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid sequence");
        after = uint32_t(parsed);
    }
    PSRAMAllocator allocator; JsonDocument doc(&allocator);
    HashCapture::json(doc, after);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return sendJsonResponse(req, doc);
}
esp_err_t POST_hash_capture(httpd_req_t *req) {
    ConGuard guard(http_server, req);
    if (is_network_allowed(req) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    if (set_cors_headers(req) != ESP_OK) return ESP_FAIL;
    if (validateOTP(req) != ESP_OK) return ESP_FAIL;
    PSRAMAllocator allocator; JsonDocument doc(&allocator);
    if (getJsonData(req, doc) != ESP_OK) return ESP_FAIL;
    if (!doc["enabled"].is<bool>())
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing enabled boolean");
    bool enabled = doc["enabled"].as<bool>();
    HashCapture::enable(enabled);
    doc.clear(); doc["enabled"] = enabled;
    return sendJsonResponse(req, doc);
}

esp_err_t GET_tuner(httpd_req_t *req) {
    ConGuard guard(http_server, req);
    if (is_network_allowed(req) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    if (set_cors_headers(req) != ESP_OK) return ESP_FAIL;
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    AutoTuner::getResultsJson(doc);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return sendJsonResponse(req, doc);
}

esp_err_t POST_tuner(httpd_req_t *req) {
    ConGuard guard(http_server, req);
    if (is_network_allowed(req) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    if (set_cors_headers(req) != ESP_OK) return ESP_FAIL;
    if (validateOTP(req) != ESP_OK) return ESP_FAIL;
    PSRAMAllocator allocator;
    JsonDocument doc(&allocator);
    esp_err_t err = getJsonData(req, doc);
    if (err != ESP_OK) return err;
    if (!doc["action"].is<const char *>())
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing action");
    int start = -1;
    if (!doc["start_pct"].isNull()) {
        if (!doc["start_pct"].is<double>())
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid start percentage");
        double value = doc["start_pct"].as<double>();
        if (!std::isfinite(value) || value < 0.01 || value > 100)
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Percentage must be 0.01..100");
        start = (int)std::round(value * 100);
    }
    int bits = -1, protocol = -1;
    if (!doc["search_bits"].isNull()) {
        if (!doc["search_bits"].is<int>() || doc["search_bits"].as<int>() < 0 || doc["search_bits"].as<int>() > 256)
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "search_bits must be 0..256");
        bits = doc["search_bits"].as<int>();
    }
    if (!doc["required_protocol"].isNull()) {
        if (!doc["required_protocol"].is<int>() || doc["required_protocol"].as<int>() < 0 || doc["required_protocol"].as<int>() > 3)
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid protocol requirement");
        protocol = doc["required_protocol"].as<int>();
    }
    int manual = -1;
    if (!doc["manual_pct"].isNull()) {
        if (!doc["manual_pct"].is<double>())
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid manual percentage");
        double value = doc["manual_pct"].as<double>();
        if (!std::isfinite(value) || value < 0 || value > 100)
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Manual percentage must be 0..100");
        manual = (int)std::round(value * 100);
    }
    if (!AutoTuner::control(doc["action"].as<const char *>(), start, bits, protocol, manual))
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid tuner command");
    doc.clear();
    AutoTuner::getResultsJson(doc);
    return sendJsonResponse(req, doc);
}
