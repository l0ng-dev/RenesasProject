#ifndef REPLAY_DATASET_H
#define REPLAY_DATASET_H

#include <stddef.h>
#include <stdint.h>

typedef struct st_replay_sample
{
    uint32_t sequence;
    char const * p_payload;
    char const * p_evidence;
} replay_sample_t;

size_t ReplayDataset_Count (void);
replay_sample_t const * ReplayDataset_Get (size_t index);

#endif
