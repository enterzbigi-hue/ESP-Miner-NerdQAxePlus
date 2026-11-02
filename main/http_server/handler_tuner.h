#pragma once
#include "esp_http_server.h"
esp_err_t GET_tuner(httpd_req_t *req);
esp_err_t POST_tuner(httpd_req_t *req);
esp_err_t GET_hash_capture(httpd_req_t *req);
esp_err_t POST_hash_capture(httpd_req_t *req);
