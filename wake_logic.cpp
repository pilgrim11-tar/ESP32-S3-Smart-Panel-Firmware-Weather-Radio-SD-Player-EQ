#include "wake_logic.h"

namespace
{
uint32_t s_eq_avg_rms = 0;
uint32_t s_donor_wake_hold_until_ms = 0;
}

void wake_logic_reset()
{
    s_eq_avg_rms = 0;
    s_donor_wake_hold_until_ms = 0;
}

bool wake_logic_update(const WakeLogicInputs &in)
{
    if (in.donor_wake_enabled)
    {
        if (in.donor_gpio_high)
        {
            s_donor_wake_hold_until_ms = in.now_ms + in.donor_wake_hold_ms;
        }
        if (in.now_ms < s_donor_wake_hold_until_ms)
        {
            return true;
        }
    }

    if (in.system_soft_off || in.media_output_active)
    {
        return false;
    }
    if (!in.donor_sound_wake_enabled || !in.mic_online)
    {
        return false;
    }
    if (in.mic_voice)
    {
        return true;
    }

    if (s_eq_avg_rms == 0)
    {
        s_eq_avg_rms = in.rms_smooth;
    }
    else
    {
        s_eq_avg_rms = (s_eq_avg_rms * 7UL + in.rms_smooth) / 8UL;
    }
    if (s_eq_avg_rms < in.noise_floor)
    {
        s_eq_avg_rms = in.noise_floor;
    }

    uint32_t threshold = s_eq_avg_rms + (in.display_sleeping ? 35UL : 55UL);
    if (in.gate_on > in.noise_floor + 12UL)
    {
        const uint32_t gateThreshold = in.noise_floor + ((in.gate_on - in.noise_floor) * 45UL) / 100UL;
        if (gateThreshold < threshold)
        {
            threshold = gateThreshold;
        }
    }
    const uint32_t floorThreshold = in.noise_floor + 60UL;
    if (threshold < floorThreshold)
    {
        threshold = floorThreshold;
    }
    if (in.display_sleeping && threshold > in.noise_floor + 50UL)
    {
        threshold -= 10UL;
    }

    uint32_t peakThreshold = threshold + 60UL;
    const uint32_t minPeakThreshold = in.noise_floor + 120UL;
    if (peakThreshold < minPeakThreshold)
    {
        peakThreshold = minPeakThreshold;
    }

    const bool rmsCandidate = in.rms_smooth >= threshold;
    const bool peakCandidate = in.peak >= peakThreshold;
    if (peakCandidate && !rmsCandidate)
    {
        uint32_t peakRmsGuard = in.noise_floor + 30UL;
        if (in.gate_on > in.noise_floor + 8UL)
        {
            const uint32_t gateGuard = in.noise_floor + ((in.gate_on - in.noise_floor) * 25UL) / 100UL;
            if (gateGuard > peakRmsGuard)
            {
                peakRmsGuard = gateGuard;
            }
        }
        if (in.rms_smooth < peakRmsGuard && in.rms_raw < (peakRmsGuard + 20UL))
        {
            return false;
        }
    }

    return rmsCandidate || peakCandidate;
}

