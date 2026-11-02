#pragma once
#include "mining.h"
#include <ArduinoJson.h>
namespace HashCapture {
void record(const bm_job *job, int asicJob, bool result = false,
            uint32_t nonce = 0, uint32_t version = 0, double diff = 0);
uint32_t inputs(const uint8_t *coinbase, size_t length, const uint8_t branches[][32],
                size_t count, size_t extranonceOffset, size_t extranonceLength);
bool isEnabled();
void enable(bool value);
void json(JsonDocument &doc, uint32_t after);
}
