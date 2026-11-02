#pragma once
#include <stdint.h>
#include "ArduinoJson.h"

namespace AutoTuner {
constexpr int TOP_RESULTS = 5;
enum class JobMode { Off, Ready, Exhausted };
void init();
// start/resume, pause, reset (starts a fresh experiment).
bool control(const char *action, int startBasisPoints = -1, int searchBits = -1, int requiredProtocol = -1, int manualBasisPoints = -1);
// Use BIP34 candidate height; repeated notifications / pools never count twice.
void observeHeight(uint32_t candidateHeight);
// Called only for local ASIC work, under the mining-info lock.
JobMode next(int pool, int bytes, char hex[65], uint32_t &run, int &step, int protocol = 1);
void reportShareDiff(uint32_t run, int step, const char *extranonce, double diff);
void getResultsJson(JsonDocument &doc);
}
