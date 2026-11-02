#include "auto_tuner.h"
#include "extranonce_range.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include <pthread.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

namespace AutoTuner {
namespace {
constexpr uint32_t VERSION = 4;
constexpr int BLOCKS = 10;
struct Hit { double diff; uint8_t value[32]; uint8_t bytes; };
struct Step {
    Hit top[TOP_RESULTS];
    uint64_t samples;
    uint64_t jobs;
    uint64_t milliseconds;
    uint32_t blocks;
    bool completed;
};
struct Cursor { uint8_t value[32]; int bytes; int step; bool exhausted; };
struct State {
    uint32_t version;
    uint32_t run;
    bool enabled;
    bool finished;
    int step;
    int startPct;
    int best;
    Step steps[20];
    Cursor cursor[2];
    int searchBits;
    int requiredProtocol;
    int manualPct;
};
State s{};
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct Lock {
    Lock() { pthread_mutex_lock(&mutex); }
    ~Lock() { pthread_mutex_unlock(&mutex); }
};
uint32_t height = 0, lastSeenHeight = 0;
int64_t lastTick = 0, lastSave = 0;
bool dirty = false, saveOk = true;
int lastBytes = 0, lastBits = 0, lastProtocol = 0;
bool lastCompatible = false;
char lastHex[65]{};
Cursor manual[2]{};
int count() { return (s.startPct + 499) / 500; }

void tick() {
    int64_t now = esp_timer_get_time();
    if (lastTick && s.enabled && s.step >= 0 && s.step < 20 && lastCompatible) {
        s.steps[s.step].milliseconds += (now - lastTick) / 1000;
        dirty = true;
    }
    lastTick = now;
}
// NVS writes must run with an internal RAM stack. Mining tasks have PSRAM
// stacks, which are inaccessible while the flash cache is disabled.
#ifdef ESP_PLATFORM
TaskHandle_t writerTask = nullptr;
bool savePending = false;
#endif
void writeState() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("en2_tuner", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, "state", &s, sizeof(s));
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    saveOk = err == ESP_OK;
    lastSave = esp_timer_get_time();
    if (saveOk) dirty = false;
    else ESP_LOGE("auto_tuner", "Cannot persist experiment: %d", (int)err);
}
#ifdef ESP_PLATFORM
void writer(void *) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        Lock lock;
        if (dirty) writeState();
        savePending = false;
    }
}
#endif
void save(bool force = false) {
    if (!dirty || (!force && esp_timer_get_time() - lastSave < 60000000)) return;
#ifdef ESP_PLATFORM
    if (!writerTask) { saveOk = false; return; }
    if (!savePending) {
        savePending = true;
        xTaskNotifyGive(writerTask);
    }
#else
    writeState();
#endif
}
void chooseBest() {
    s.best = -1;
    for (int i = 0; i < 20; ++i) {
        if (s.steps[i].top[0].diff > 0 &&
            (s.best < 0 || s.steps[i].top[0].diff > s.steps[s.best].top[0].diff)) s.best = i;
    }
}
}

void init() {
    Lock lock;
    s = State{};
    memset(manual, 0, sizeof(manual));
    lastBytes = lastBits = lastProtocol = 0;
    lastCompatible = false;
    lastHex[0] = 0;
    s.version = VERSION;
    s.run = 1;
    s.startPct = 10000;
    s.searchBits = 32;
    s.manualPct = 7324;
    s.best = -1;
    nvs_handle_t handle;
    if (nvs_open("en2_tuner", NVS_READONLY, &handle) == ESP_OK) {
        size_t size = 0;
        State *restored = (State *)calloc(1, sizeof(State));
        const size_t legacySize = offsetof(State, searchBits);
        const size_t v3Size = (offsetof(State, manualPct) + alignof(State) - 1) / alignof(State) * alignof(State);
        if (restored && nvs_get_blob(handle, "state", nullptr, &size) == ESP_OK &&
            (size == sizeof(State) || size == legacySize || size == v3Size) &&
            nvs_get_blob(handle, "state", restored, &size) == ESP_OK &&
            ((restored->version == 2 && size == legacySize) ||
             (restored->version == 3 && size == v3Size) ||
             (restored->version == VERSION && size == sizeof(State))) &&
            restored->step >= 0 && restored->step < 20 &&
            restored->best >= -1 && restored->best < 20 && restored->run != 0 &&
            restored->startPct > 0 && restored->startPct <= 10000 &&
            (restored->version < 4 || (restored->manualPct >= 0 && restored->manualPct <= 10000)) &&
            restored->searchBits >= 0 && restored->searchBits <= 256 &&
            restored->requiredProtocol >= 0 && restored->requiredProtocol <= 3 &&
            restored->step < (restored->startPct + 499) / 500) {
            if (restored->version == 2) {
                // Preserve the old full-session domain and ranking on upgrade.
                memset(restored->cursor, 0, sizeof(restored->cursor));
                dirty = true;
            }
            if (restored->version < 4) {
                restored->manualPct = 7324;
                restored->version = VERSION;
                dirty = true;
            }
            s = *restored;
        }
        free(restored);
        nvs_close(handle);
    }
    // Downtime is never attributed to a range. Reconnect supplies a fresh baseline.
    height = lastSeenHeight = 0;
    lastTick = esp_timer_get_time();
#ifdef ESP_PLATFORM
    if (!writerTask && xTaskCreate(writer, "tuner_nvs", 4096, nullptr, 1, &writerTask) != pdPASS) {
        writerTask = nullptr;
        saveOk = false;
        ESP_LOGE("auto_tuner", "Cannot create internal-stack NVS writer");
    }
#endif
}

bool control(const char *action, int startBasisPoints, int searchBits, int requiredProtocol, int manualBasisPoints) {
    Lock lock;
    tick();
    if (startBasisPoints != -1 && (startBasisPoints < 1 || startBasisPoints > 10000)) return false;
    if (manualBasisPoints < -1 || manualBasisPoints > 10000 || (manualBasisPoints != -1 && strcmp(action, "pause"))) return false;
    if (searchBits < -1 || searchBits > 256 || requiredProtocol < -1 || requiredProtocol > 3) return false;
    if ((startBasisPoints != -1 || searchBits != -1 || requiredProtocol != -1) && strcmp(action, "reset")) return false;
    if (!strcmp(action, "reset")) {
        int start = startBasisPoints == -1 ? s.startPct : startBasisPoints;
        int bits = searchBits == -1 ? s.searchBits : searchBits;
        int protocol = requiredProtocol == -1 ? s.requiredProtocol : requiredProtocol;
        int manualPct = s.manualPct;
        uint32_t run = s.run + 1;
        s = State{};
        s.version = VERSION;
        s.run = run ? run : 1;
        s.best = -1;
        s.startPct = start;
        s.searchBits = bits;
        s.requiredProtocol = protocol;
        s.manualPct = manualPct;
        s.enabled = true;
    } else if (!strcmp(action, "start")) {
        if (s.finished && s.best < 0) return false;
        s.enabled = true;
    } else if (!strcmp(action, "pause")) {
        if (manualBasisPoints != -1) s.manualPct = manualBasisPoints;
        s.enabled = false;
        memset(manual, 0, sizeof(manual));
    } else return false;
    // Use the latest known tip so the first block after enabling counts immediately.
    height = lastSeenHeight;
    lastCompatible = false;
    lastTick = esp_timer_get_time();
    dirty = true;
    save(true);
    return true;
}

void observeHeight(uint32_t candidateHeight) {
    Lock lock;
    if (!candidateHeight) return;
    if (candidateHeight > lastSeenHeight) lastSeenHeight = candidateHeight;
    if (!s.enabled || s.finished || !lastCompatible) return;
    if (!height) { height = candidateHeight; return; }
    if (candidateHeight <= height) return;
    // One observed forward transition. A reconnect gap must not finish untested ranges.
    height = candidateHeight;
    tick();
    Step &step = s.steps[s.step];
    ++step.blocks;
    if (step.blocks >= BLOCKS) {
        step.completed = true;
        if (s.step < count() - 1) ++s.step;
        else {
            s.finished = true;
            chooseBest();
            if (s.best >= 0) s.step = s.best;
            else { s.enabled = false; memset(manual, 0, sizeof(manual)); }
        }
    }
    dirty = true;
    save(true);
}

JobMode next(int pool, int bytes, char hex[65], uint32_t &run, int &step, int protocol) {
    Lock lock;
    tick();
    run = 0; step = -1;
    lastBytes = (bytes > 0 && bytes <= 32) ? bytes : 0;
    lastProtocol = protocol;
    lastBits = s.searchBits ? s.searchBits : lastBytes * 8;
    lastCompatible = lastBytes > 0 && lastBits <= lastBytes * 8 &&
        (!s.requiredProtocol || s.requiredProtocol == protocol);
    if (!lastCompatible) lastBits = 0;
    if (!lastBytes) lastHex[0] = 0;
    save();
    if (pool < 0 || pool > 1 || !lastBytes) return JobMode::Off;
    if (!s.enabled || !lastCompatible) {
        Cursor &c = manual[pool];
        // Percentages use the chosen counter domain, while the wire field
        // always retains the length negotiated with the pool.
        const int bits = s.searchBits > 0 && s.searchBits <= bytes * 8 ? s.searchBits : bytes * 8;
        const int valueBytes = (bits + 7) / 8;
        if (c.bytes != bits || c.step != bytes) {
            c = Cursor{};
            c.bytes = bits;
            c.step = bytes;
            ExtranonceRange::percentage(bits, s.manualPct, c.value);
        }
        if (c.exhausted) return JobMode::Exhausted;
        uint8_t wire[32]{};
        memcpy(wire + bytes - valueBytes, c.value, valueBytes);
        ExtranonceRange::hex(wire, bytes, hex);
        strcpy(lastHex, hex);
        uint8_t zero[32]{};
        if (!memcmp(c.value, zero, valueBytes)) c.exhausted = true;
        else ExtranonceRange::decrement(c.value, valueBytes);
        return JobMode::Ready;
    }
    Cursor &c = s.cursor[pool];
    uint8_t low[32]{}, high[32]{};
    const int valueBytes = (lastBits + 7) / 8;
    if (!ExtranonceRange::boundsBits(lastBits, s.step, low, high, s.startPct)) return JobMode::Exhausted;
    if (c.bytes != lastBits || c.step != s.step) {
        c = Cursor{};
        c.bytes = lastBits;
        c.step = s.step;
        memcpy(c.value, high, valueBytes);
    }
    if (c.exhausted) return JobMode::Exhausted;
    uint8_t wire[32]{};
    memcpy(wire + bytes - valueBytes, c.value, valueBytes);
    ExtranonceRange::hex(wire, bytes, hex);
    strcpy(lastHex, hex);
    if (!memcmp(c.value, low, valueBytes)) c.exhausted = true;
    else ExtranonceRange::decrement(c.value, valueBytes);
    // Continue collecting results while holding the selected range.
    run = s.run; step = s.step; ++s.steps[s.step].jobs;
    dirty = true;
    return JobMode::Ready;
}

void reportShareDiff(uint32_t run, int step, const char *extranonce, double diff) {
    if (!run || step < 0 || step >= 20 || !extranonce || !std::isfinite(diff) || diff <= 0) return;
    Lock lock;
    if (run != s.run) return;
    Step &r = s.steps[step];
    ++r.samples;
    for (int i = 0; i < TOP_RESULTS; ++i) {
        if (diff > r.top[i].diff) {
            for (int j = TOP_RESULTS - 1; j > i; --j) r.top[j] = r.top[j-1];
            r.top[i].diff = diff;
            size_t bytes = strlen(extranonce) / 2;
            if (bytes > 32) bytes = 32;
            r.top[i].bytes = bytes;
            for (size_t b = 0; b < bytes; ++b) {
                unsigned value = 0;
                sscanf(extranonce + b*2, "%2x", &value);
                r.top[i].value[b] = value;
            }
            break;
        }
    }
    // A delayed result can still improve its original range after the scan ends.
    chooseBest();
    if (s.finished && s.best >= 0) s.step = s.best;
    dirty = true;
    save();
}

void getResultsJson(JsonDocument &doc) {
    Lock lock;
    tick();
    save();
    doc["search_bits"] = s.searchBits;
    doc["required_protocol"] = s.requiredProtocol;
    doc["session_protocol"] = lastProtocol;
    doc["effective_bits"] = lastBits;
    doc["compatible"] = lastCompatible;
    doc["start_pct"] = s.startPct / 100.0;
    doc["manual_pct"] = s.manualPct / 100.0;
    doc["step_count"] = count();
    doc["enabled"] = s.enabled;
    doc["finished"] = s.finished;
    doc["current_step"] = s.step;
    doc["best_step"] = s.best;
    doc["blocks_target"] = BLOCKS;
    doc["candidate_height"] = height;
    doc["extranonce_bytes"] = lastBytes;
    doc["current_hex"] = lastHex;
    doc["persistence_ok"] = saveOk;
    doc["checkpoint_seconds"] = 60;
    doc["scope"] = "local_asic";
    JsonArray rows = doc["steps"].to<JsonArray>();
    for (int i = 0; i < count(); ++i) {
        const Step &r = s.steps[i];
        JsonObject row = rows.add<JsonObject>();
        row["index"] = i;
        row["upper_pct"] = (s.startPct - i*500) / 100.0;
        row["lower_pct"] = (s.startPct > (i+1)*500 ? s.startPct - (i+1)*500 : 0) / 100.0;
        row["completed"] = r.completed;
        row["blocks"] = r.blocks;
        row["samples"] = r.samples;
        row["jobs"] = r.jobs;
        row["seconds"] = r.milliseconds / 1000;
        row["max_diff"] = r.top[0].diff;
        row["is_best"] = i == s.best;
        row["exhausted"] = false;
        if (i == s.step) row["exhausted"] = s.cursor[0].exhausted || s.cursor[1].exhausted;
        if (lastBits > 0 && lastBytes > 0) {
            uint8_t low[32]{}, high[32]{}, wire[32]{};
            char text[65];
            int valueBytes = (lastBits + 7) / 8;
            row["empty"] = !ExtranonceRange::boundsBits(lastBits, i, low, high, s.startPct);
            memcpy(wire + lastBytes - valueBytes, low, valueBytes);
            ExtranonceRange::hex(wire, lastBytes, text); row["low_hex"] = text;
            memcpy(wire + lastBytes - valueBytes, high, valueBytes);
            ExtranonceRange::hex(wire, lastBytes, text); row["high_hex"] = text;
        }
        JsonArray top = row["top"].to<JsonArray>();
        for (const Hit &hit : r.top) if (hit.diff > 0) {
            JsonObject item = top.add<JsonObject>();
            item["diff"] = hit.diff;
            char hitHex[65];
            ExtranonceRange::hex(hit.value, hit.bytes <= 32 ? hit.bytes : 0, hitHex);
            item["hex"] = hitHex;
        }
    }
}
}
