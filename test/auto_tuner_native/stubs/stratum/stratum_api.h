#pragma once
#include <stdint.h>
#include <stddef.h>
#define HASH_SIZE 32
struct mining_notify {
  char *job_id;
  uint8_t _prev_block_hash[32];
  char *coinbase_1;
  char *coinbase_2;
  uint8_t _merkle_branches[32][32];
  size_t n_merkle_branches;
  uint32_t version, version_mask, target, ntime, difficulty;
};
