# Auto tuner (extranonce2)

The Auto tuner tab lets you choose which extranonce2 values your NerdQAxe sends, scan ranges of them, and keep statistics of the best difficulty found in each range. It's for people who like to experiment with their miner and see the numbers.

> **Honest note.** Bitcoin proof-of-work behaves like a random oracle. No extranonce2 range is "luckier" than another, and the best diff of a range is a historical record, not a prediction. The tuner doesn't raise your chance of finding a block. It lets you control and measure, and the panel says so too.

## Where to find it

AxeOS side menu → **Auto tuner** (`/#/pages/tuner`). The tab follows the panel language (9 languages).

## What you can set

- **Extranonce2 counter width:** 4, 8, 16, 32, 64, 128 or 256 bits, or **Auto** (the whole allocation from the pool). The default is 32 bits = 4 bytes = `FFFFFFFF`. A smaller counter is zero-padded to the length the pool expects. A larger one than the pool allows is refused, and normal mining continues.
- **Start when off (%):** with the tuner off, the counter starts at this percentage of the selected space and counts **down** by 1 per job. It never wraps upward. The default is 73.24%, which is `BB7E90FE` for 4 bytes.
- **New scan start (%):** 0.01–100%, two decimal places. **Start new measurement** clears the previous results, so export them first.
- **Sessions counted:** Auto (V1 or V2 Extended), V1 only, V2 Extended only. V2 Standard can mine, but it doesn't expose extranonce2.
- **Pool connection** (collapsible section): Stratum V1, V2 Standard or V2 Extended for the primary or fallback pool. Saving reconnects to the pool.

## How a scan works

1. The scan walks down in 5-percentage-point ranges: 100–95%, 95–90%, … 5–0%. With a custom start such as 73.24% the ranges are 73.24–68.24%, … and the last one can be narrower.
2. Inside a range, extranonce2 goes down by 1 for every new job.
3. A range lasts **10 observed changes of the Bitcoin block height**. The height is read from the coinbase of the pool's jobs, so several jobs for the same block, or the same block from two pools, count once.
4. After the last range, the tuner keeps working in the range with the highest recorded diff and keeps collecting results. On a tie, the first range in scan order wins. If the whole scan had no valid ASIC hit, the tuner switches itself off and returns to the manual start.

## Statistics

For every range the panel shows blocks, jobs, valid ASIC samples, time spent, the best diff and the **top 5 diffs with their exact extranonce2**. Diffs are stored as `double`, so they aren't truncated to 32 bits. **Export CSV** saves everything.

State, counters and the ranking are saved in their own NVS namespace every 60 seconds, and also on mode changes and new blocks. After a reboot the experiment continues where it was. A sudden power loss can lose the last minute. A failed save is shown in the panel.

## API

| Method | Path | What it does |
|---|---|---|
| `GET` | `/api/v2/tuner` | Current state, settings and all range statistics (JSON). |
| `POST` | `/api/v2/tuner` | `{"action":"start"}` · `{"action":"pause","manual_pct":45.5}` · `{"action":"reset","start_pct":70,"search_bits":32,"required_protocol":0}` |
| `GET` | `/api/v2/hash-capture` | The last 32 jobs and hits with full inputs (coinbase, merkle branches, 80-byte header), kept in PSRAM. |
| `POST` | `/api/v2/hash-capture` | `{"enabled": true}` or `{"enabled": false}`: turn the capture on or off. |

Changes go through the existing OTP protection when OTP is enabled.

## Limits

- Only the device's own ASICs take part. CAN slave devices keep their normal extranonce2 handling.
- When a range is used up, the counter waits for the next range instead of repeating work.
- In V2 Standard the tuner can't change extranonce2, because the pool builds the coinbase.

## Tests

`python3 test/run_auto_tuner_native.py` (needs Python 3 and a C++17 compiler) runs the tuner and the job builders natively with AddressSanitizer and UndefinedBehaviorSanitizer. It also checks 17,280 ranges against Python big-integer math at every width from 1 to 256 bits. The GitHub workflow **NerdQAxe++ Auto Tuner - BIN** runs these tests and then builds `esp-miner.bin` and `www.bin` for `NERDQAXEPLUS2` with ESP-IDF 5.3.6.
