#pragma once
#include <vector>
#include <cstring>
using nvs_handle_t = int;
using esp_err_t = int;
constexpr int ESP_OK = 0, NVS_READWRITE = 1, NVS_READONLY = 0;
extern std::vector<unsigned char> fake_nvs;
inline int nvs_open(const char *, int, int *handle) { *handle = 1; return 0; }
inline int nvs_set_blob(int, const char *, const void *data, size_t size) {
  fake_nvs.assign((const unsigned char *)data, (const unsigned char *)data + size); return 0;
}
inline int nvs_get_blob(int, const char *, void *data, size_t *size) {
  if (fake_nvs.empty()) return -1;
  if (!data) { *size=fake_nvs.size(); return 0; }
  if (*size < fake_nvs.size()) return -1;
  *size=fake_nvs.size(); memcpy(data, fake_nvs.data(), *size); return 0;
}
inline int nvs_commit(int) { return 0; }
inline void nvs_close(int) {}
