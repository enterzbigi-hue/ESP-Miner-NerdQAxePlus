#include "hash_capture.h"
#include "block_header.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <atomic>
#include <pthread.h>
#include <stdio.h>
#include <memory>
#include <cstring>
#include "esp_heap_caps.h"

namespace {
constexpr unsigned CAPACITY = 32;
struct Inputs {
    uint32_t id; size_t length, count, offset, extraLength;
    uint8_t *bytes;
    ~Inputs() { heap_caps_free(bytes); }
};
std::shared_ptr<Inputs> inputRing[CAPACITY];
uint32_t inputLatest = 0;
struct Record {
    uint32_t sequence, run, inputId, versionMask, poolDiff, asicDiff;
    int64_t uptimeUs;
    int pool, asicJob, step;
    bool result;
    double diff;
    uint8_t header[80];
    char extranonce[65], poolJob[65];
};
Record ring[CAPACITY];
uint32_t latest = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
std::atomic<bool> enabled{false};
void hex(const uint8_t *bytes, size_t size, char *out, bool reverse = false) {
    static const char chars[] = "0123456789abcdef";
    for (size_t i = 0; i < size; ++i) {
        uint8_t b = bytes[reverse ? size - 1 - i : i];
        out[2*i] = chars[b >> 4]; out[2*i+1] = chars[b & 15];
    }
    out[size*2] = 0;
}
}
uint32_t HashCapture::inputs(const uint8_t *coinbase, size_t length,
                            const uint8_t branches[][32], size_t count,
                            size_t extranonceOffset, size_t extranonceLength) {
    if (!enabled.load() || !coinbase || !length || length > 16384 || count > 32 ||
        (count && !branches) || extranonceOffset > length ||
        extranonceLength > length - extranonceOffset) return 0;
    auto bytes = static_cast<uint8_t *>(heap_caps_malloc(length + count * 32,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!bytes) return 0; // Telemetry allocation must never stop mining.
    memcpy(bytes, coinbase, length);
    if (count) memcpy(bytes + length, branches, count * 32);
    auto raw = new (std::nothrow) Inputs{};
    if (!raw) { heap_caps_free(bytes); return 0; }
    std::shared_ptr<Inputs> value(raw);
    value->bytes = bytes; value->length = length; value->count = count;
    value->offset = extranonceOffset; value->extraLength = extranonceLength;
    pthread_mutex_lock(&mutex);
    value->id = ++inputLatest; inputRing[(inputLatest - 1) % CAPACITY] = value;
    pthread_mutex_unlock(&mutex);
    return value->id;
}
bool HashCapture::isEnabled() { return enabled.load(); }
void HashCapture::enable(bool value) { enabled.store(value); }
void HashCapture::record(const bm_job *job, int asicJob, bool result,
                        uint32_t nonce, uint32_t version, double diff) {
    if (!enabled.load() || !job) return;
    Record r{};
    r.uptimeUs = esp_timer_get_time(); r.pool = job->pool_id;
    r.asicJob = asicJob; r.step = job->tuner_step; r.run = job->tuner_run;
    r.versionMask = job->version_mask; r.poolDiff = job->pool_diff; r.asicDiff = job->asic_diff;
    r.result = result; r.diff = diff; r.inputId = job->capture_inputs_id;
    serialize_block_header(r.header, result ? version : job->version,
        job->prev_block_hash, job->merkle_root, job->ntime, job->target,
        result ? nonce : job->starting_nonce);
    snprintf(r.extranonce, sizeof(r.extranonce), "%s", job->extranonce2 ? job->extranonce2 : "");
    snprintf(r.poolJob, sizeof(r.poolJob), "%s", job->jobid ? job->jobid : "");
    pthread_mutex_lock(&mutex);
    r.sequence = ++latest; ring[(latest - 1) % CAPACITY] = r;
    pthread_mutex_unlock(&mutex);
}
void HashCapture::json(JsonDocument &doc, uint32_t after) {
    static const uint32_t boot = esp_random();
    uint32_t last;
    pthread_mutex_lock(&mutex); last = latest; pthread_mutex_unlock(&mutex);
    const uint32_t first = last >= CAPACITY ? last - CAPACITY + 1 : 1;
    doc["schema"] = 2; doc["boot_id"] = boot; doc["enabled"] = enabled.load();
    doc["capacity"] = CAPACITY; doc["latest_sequence"] = last;
    doc["oldest_sequence"] = first;
    doc["gap_records"] = first > after && first - after > 1 ? first - after - 1 : 0;
    JsonArray rows = doc["records"].to<JsonArray>();
    for (uint32_t seq = after >= last ? last + 1 : (after + 1 < first ? first : after + 1); seq <= last; ++seq) {
        Record r;
        pthread_mutex_lock(&mutex); r = ring[(seq - 1) % CAPACITY]; pthread_mutex_unlock(&mutex);
        if (r.sequence != seq) continue; // Overwritten while serializing: visible as a sequence gap.
        JsonObject row = rows.add<JsonObject>();
        row["sequence"] = seq; row["device_uptime_us"] = r.uptimeUs;
        row["type"] = r.result ? "result" : "job";
        row["pool_slot"] = r.pool; row["pool_job_id"] = r.poolJob;
        row["asic_job_id"] = r.asicJob; row["tuner_run"] = r.run; row["tuner_step"] = r.step;
        row["extranonce2_hex"] = r.extranonce;
        char text[161]; hex(r.header, 80, text); row["header_hex"] = text;
        hex(r.header + 36, 32, text); row["merkle_root_internal_hex"] = text;
        hex(r.header + 36, 32, text, true); row["merkle_root_display_hex"] = text;
        row["capture_inputs_id"] = r.inputId;
        row["version_rolling_mask"] = r.versionMask;
        row["pool_difficulty"] = r.poolDiff; row["asic_difficulty"] = r.asicDiff;
        std::shared_ptr<Inputs> input;
        if (r.inputId) {
            pthread_mutex_lock(&mutex);
            input = inputRing[(r.inputId - 1) % CAPACITY];
            pthread_mutex_unlock(&mutex);
            if (input && input->id != r.inputId) input.reset();
        }
        row["full_inputs_available"] = bool(input);
        if (input) {
            // Large buffers live in PSRAM, never on the HTTP task stack.
            auto buffer = static_cast<char *>(heap_caps_malloc(input->length * 2 + 1,
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (buffer) {
                hex(input->bytes, input->length, buffer); row["coinbase_hex"] = buffer;
                heap_caps_free(buffer);
                row["extranonce2_offset_bytes"] = input->offset;
                row["extranonce2_length_bytes"] = input->extraLength;
                JsonArray branches = row["merkle_branches_internal_hex"].to<JsonArray>();
                for (size_t i = 0; i < input->count; ++i) {
                    hex(input->bytes + input->length + i * 32, 32, text); branches.add(text);
                }
            } else row["full_inputs_available"] = false;
        }
        if (r.result) row["device_diff"] = r.diff;
    }
}
