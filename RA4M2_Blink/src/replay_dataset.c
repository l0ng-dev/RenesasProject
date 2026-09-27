#include "replay_dataset.h"

/*
 * Fixed samples transcribed from the PondSense handover report. They are
 * historical evidence records, not measurements made by RA4M2. The pH sample
 * remains explicitly marked as an uncalibrated demonstration estimate.
 *
 * Evidence source:
 * E:\PondSense\智能鱼类投喂器项目交接报告_2026-09-12.md
 * - section 2.4: reported 23.89 C water-temperature reading
 * - section 2.6: photo-visible 23.56 C and temporary pH 6.80 estimate
 *
 * Each payload stays below 100 bytes so it also fits the conservative DPM
 * MQTT message limit documented for some DA16200 configurations.
 */
static replay_sample_t const g_replay_samples[] =
{
    {
        1U,
        "{\"seq\":1,\"src\":\"hist\",\"temp_centi\":2389,\"ph_centi\":null,\"feed\":null,\"q\":\"reported\"}",
        "PondSense handover section 2.4"
    },
    {
        2U,
        "{\"seq\":2,\"src\":\"hist\",\"temp_centi\":2356,\"ph_centi\":null,\"feed\":null,\"q\":\"photo\"}",
        "PondSense handover section 2.6"
    },
    {
        3U,
        "{\"seq\":3,\"src\":\"hist\",\"temp_centi\":null,\"ph_centi\":680,\"feed\":null,\"q\":\"demo_uncal\"}",
        "PondSense handover section 2.6"
    }
};

size_t ReplayDataset_Count (void)
{
    return sizeof(g_replay_samples) / sizeof(g_replay_samples[0]);
}

replay_sample_t const * ReplayDataset_Get (size_t index)
{
    if (index >= ReplayDataset_Count())
    {
        return NULL;
    }

    return &g_replay_samples[index];
}
