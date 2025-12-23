#pragma once

#include <stdint.h>
#include <condition_variable>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <stop_token>

using Lock = std::shared_mutex;
using WriteLock = std::unique_lock<Lock>;
using ReadLock = std::shared_lock<Lock>;
using ConditionVar = std::condition_variable_any;

struct SampleChunk {
  uint8_t* data;
  int num_samples;
};

class SampleQueue {
 public:
  SampleQueue(std::stop_token token,
              int max_samples,
              int chunk_samples,
              int sample_bytes);

  bool is_full();

  // Returns whether the remaining samples in the queue
  // don't make up a full chunk
  bool partial_chunk_remaining();

  // Push a variable number of samples to the queue.
  // This will block until there's enough space in the queue.
  void push(uint8_t* samples, int num_samples);

  // Read back samples from the queue in a fixed sized chunk.
  // This will block until there is a valid chunk in the queue.
  // NOTE: The caller will be responsible for freeing the data
  // refered to by the chunk.
  SampleChunk pop(bool allow_partial_chunk);

 private:
  bool can_write(int samples_to_write);
  bool can_read(bool allow_partial);

  Lock m_lock;
  ConditionVar m_cond_not_full;
  ConditionVar m_cond_not_empty;
  std::stop_token m_stop_token;

  int m_max_chunks;
  int m_sample_bytes;
  int m_chunk_samples;
  std::list<SampleChunk> m_chunks;
};
