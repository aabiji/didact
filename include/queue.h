#pragma once

#include <stdlib.h>
#include <list>

struct SampleChunk {
  int16_t* samples;
  int num_samples;
};

class SampleQueue {
 public:
  ~SampleQueue();
  void init(int max_chunks, int chunk_samples);

  bool is_full();
  bool has_chunk();

  // Returns whether the remaining samples in the queue
  // don't make up a full chunk
  bool partial_chunk_remaining();

  // Push a variable number of samples to the queue.
  // This will block until there's enough space in the queue.
  void push_samples(int16_t* samples, int num_samples);

  // Read back samples from the queue in a fixed sized chunk.
  // This will block until there is a valid chunk in the queue.
  // NOTE: The caller will be responsible for freeing the data
  // refered to by the chunk.
  SampleChunk pop_chunk(bool allow_partial_chunk);

 private:
  int m_max_chunks;
  int m_chunk_samples;
  std::list<SampleChunk> m_chunks;
};
