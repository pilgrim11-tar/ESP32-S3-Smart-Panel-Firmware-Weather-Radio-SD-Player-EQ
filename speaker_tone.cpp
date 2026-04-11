#include "speaker_tone.h"

#include <math.h>

#include <driver/i2s.h>

static volatile bool s_tone_busy = false;
static volatile uint8_t s_tone_mode = 0;
static volatile bool s_tone_stop_requested = false;
static volatile uint8_t s_alarm_intensity = 0;
static volatile uint8_t s_alarm_melody = 1;

static constexpr i2s_port_t kToneI2S = I2S_NUM_1;
static constexpr uint32_t kToneSampleRate = 22050;
static constexpr int16_t kMaxToneAmplitude = 14000;

static float clamp_amp(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > (float)kMaxToneAmplitude)
    {
        return (float)kMaxToneAmplitude;
    }
    return value;
}

static void fill_stereo_sine_ramp(int16_t *buffer,
                                  size_t frames,
                                  float frequency_hz,
                                  uint32_t sample_rate,
                                  float *phase,
                                  float amplitude_start,
                                  float amplitude_end)
{
    const float phase_step = 2.0f * PI * frequency_hz / (float)sample_rate;
    const float amp0 = clamp_amp(amplitude_start);
    const float amp1 = clamp_amp(amplitude_end);
    const float amp_step = (frames > 1) ? ((amp1 - amp0) / (float)(frames - 1)) : 0.0f;
    float amp = amp0;

    const size_t edge = (frames > 32U) ? min<size_t>(frames / 8U, 80U) : 0U;

    for (size_t i = 0; i < frames; ++i)
    {
        float env = 1.0f;
        if (edge > 0U)
        {
            if (i < edge)
            {
                env = (float)(i + 1U) / (float)edge;
            }
            else if (i >= (frames - edge))
            {
                env = (float)(frames - i) / (float)edge;
            }
        }

        int32_t sample = (int32_t)(sinf(*phase) * amp * env);
        if (sample > 32767)
        {
            sample = 32767;
        }
        else if (sample < -32768)
        {
            sample = -32768;
        }

        buffer[i * 2] = (int16_t)sample;
        buffer[i * 2 + 1] = (int16_t)sample;

        amp += amp_step;
        *phase += phase_step;
        if (*phase >= 2.0f * PI)
        {
            *phase -= 2.0f * PI;
        }
    }
}

static bool install_i2s()
{
    i2s_driver_uninstall(kToneI2S);

    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = kToneSampleRate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = 256;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    cfg.fixed_mclk = 0;

    i2s_pin_config_t pins = {};
    pins.bck_io_num = 1;
    pins.ws_io_num = 2;
    pins.data_out_num = 40;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    if (i2s_driver_install(kToneI2S, &cfg, 0, NULL) != ESP_OK)
    {
        return false;
    }
    if (i2s_set_pin(kToneI2S, &pins) != ESP_OK)
    {
        i2s_driver_uninstall(kToneI2S);
        return false;
    }
    i2s_zero_dma_buffer(kToneI2S);
    return true;
}

static bool play_tone(float frequency_hz, uint32_t duration_ms, float amplitude_start, float amplitude_end)
{
    static int16_t buffer[256 * 2];
    size_t written = 0;
    float phase = 0.0f;

    uint32_t total_frames = (kToneSampleRate * duration_ms) / 1000U;
    if (total_frames == 0)
    {
        total_frames = 1;
    }

    uint32_t frames_done = 0;
    while (frames_done < total_frames)
    {
        if (s_tone_stop_requested)
        {
            return false;
        }

        const uint32_t frames_left = total_frames - frames_done;
        const size_t frames = (frames_left > 256U) ? 256U : (size_t)frames_left;
        const float t0 = (float)frames_done / (float)total_frames;
        const float t1 = (float)(frames_done + (uint32_t)frames) / (float)total_frames;
        const float amp0 = amplitude_start + (amplitude_end - amplitude_start) * t0;
        const float amp1 = amplitude_start + (amplitude_end - amplitude_start) * t1;

        fill_stereo_sine_ramp(buffer, frames, frequency_hz, kToneSampleRate, &phase, amp0, amp1);
        if (i2s_write(kToneI2S, buffer, frames * sizeof(int16_t) * 2U, &written, portMAX_DELAY) != ESP_OK)
        {
            return false;
        }

        frames_done += (uint32_t)frames;
    }

    return !s_tone_stop_requested;
}

static bool play_silence(uint32_t duration_ms)
{
    static int16_t zeros[256 * 2] = {};
    size_t written = 0;

    uint32_t total_frames = (kToneSampleRate * duration_ms) / 1000U;
    if (total_frames == 0)
    {
        return true;
    }

    while (total_frames > 0)
    {
        if (s_tone_stop_requested)
        {
            return false;
        }

        const size_t frames = (total_frames > 256U) ? 256U : (size_t)total_frames;
        if (i2s_write(kToneI2S, zeros, frames * sizeof(int16_t) * 2U, &written, portMAX_DELAY) != ESP_OK)
        {
            return false;
        }
        total_frames -= (uint32_t)frames;
    }

    return !s_tone_stop_requested;
}

static bool play_alarm_melody(uint8_t melody_id, uint8_t intensity_level)
{
    static const float melody1_notes[] = {523.25f, 659.25f, 783.99f, 659.25f, 880.00f, 783.99f, 659.25f, 587.33f};
    static const uint16_t melody1_durations[] = {150, 150, 170, 130, 180, 160, 130, 220};

    static const float melody2_notes[] = {880.00f, 988.00f, 1046.50f, 988.00f, 880.00f, 784.00f, 659.25f, 784.00f};
    static const uint16_t melody2_durations[] = {120, 120, 140, 120, 120, 140, 180, 220};

    static const float melody3_notes[] = {1318.50f, 1318.50f, 1046.50f, 1318.50f, 1567.98f, 1318.50f, 1046.50f, 880.00f};
    static const uint16_t melody3_durations[] = {90, 90, 120, 100, 150, 120, 120, 260};

    const float *notes = melody1_notes;
    const uint16_t *durations = melody1_durations;
    size_t note_count = sizeof(melody1_notes) / sizeof(melody1_notes[0]);
    uint16_t gap_ms = 34;
    uint16_t tail_ms = 90;
    float amplitude_step = 850.0f;

    if (melody_id == 2U)
    {
        notes = melody2_notes;
        durations = melody2_durations;
        note_count = sizeof(melody2_notes) / sizeof(melody2_notes[0]);
        gap_ms = 26;
        tail_ms = 80;
        amplitude_step = 760.0f;
    }
    else if (melody_id == 3U)
    {
        notes = melody3_notes;
        durations = melody3_durations;
        note_count = sizeof(melody3_notes) / sizeof(melody3_notes[0]);
        gap_ms = 24;
        tail_ms = 120;
        amplitude_step = 900.0f;
    }

    const uint8_t level = (intensity_level > 12U) ? 12U : intensity_level;
    float amplitude = 800.0f + (float)level * 650.0f;

    for (size_t i = 0; i < note_count; ++i)
    {
        const float next_amplitude = clamp_amp(amplitude + amplitude_step);
        if (!play_tone(notes[i], durations[i], amplitude, next_amplitude))
        {
            return false;
        }
        if ((i + 1U) < note_count && !play_silence(gap_ms))
        {
            return false;
        }
        amplitude = next_amplitude;
    }

    return play_silence(tail_ms);
}

static void speaker_tone_task(void *parameter)
{
    (void)parameter;

    if (!install_i2s())
    {
        Serial.println("[tone] i2s install failed");
        s_tone_busy = false;
        s_tone_mode = 0;
        s_tone_stop_requested = false;
        vTaskDelete(NULL);
        return;
    }

    if (s_tone_mode == 1)
    {
        (void)play_tone(392.0f, 10, 950.0f, 950.0f);
    }
    else if (s_tone_mode == 2)
    {
        (void)play_tone(659.25f, 40, 1300.0f, 1600.0f);
        (void)play_silence(18);
        (void)play_tone(880.0f, 55, 1700.0f, 2100.0f);
    }
    else if (s_tone_mode == 3)
    {
        (void)play_alarm_melody(s_alarm_melody, s_alarm_intensity);
    }
    else
    {
        (void)play_tone(523.25f, 240, 1200.0f, 1700.0f);
        (void)play_silence(70);
        (void)play_tone(659.25f, 240, 1500.0f, 2000.0f);
        (void)play_silence(70);
        (void)play_tone(783.99f, 320, 1700.0f, 2400.0f);
        (void)play_silence(120);
        (void)play_tone(659.25f, 220, 1500.0f, 2100.0f);
        (void)play_silence(60);
        (void)play_tone(523.25f, 320, 1200.0f, 1800.0f);
    }

    i2s_zero_dma_buffer(kToneI2S);
    i2s_driver_uninstall(kToneI2S);

    s_tone_mode = 0;
    s_tone_busy = false;
    s_tone_stop_requested = false;
    vTaskDelete(NULL);
}

static bool start_mode(uint8_t mode, uint8_t alarm_intensity, uint8_t alarm_melody)
{
    if (s_tone_busy)
    {
        return false;
    }

    s_tone_mode = mode;
    s_alarm_intensity = alarm_intensity;
    s_alarm_melody = (alarm_melody >= 1U && alarm_melody <= 3U) ? alarm_melody : 1U;
    s_tone_stop_requested = false;
    s_tone_busy = true;

    BaseType_t ok = xTaskCreatePinnedToCore(speaker_tone_task,
                                            (mode == 3U) ? "speaker_alarm_task" : "speaker_tone_task",
                                            4096,
                                            NULL,
                                            2,
                                            NULL,
                                            0);
    if (ok != pdPASS)
    {
        s_tone_mode = 0;
        s_tone_busy = false;
        s_tone_stop_requested = false;
        s_alarm_intensity = 0;
        s_alarm_melody = 1;
        return false;
    }

    return true;
}

bool speaker_tone_start()
{
    return start_mode(0, 0, 1);
}

bool speaker_tone_is_busy()
{
    return s_tone_busy;
}

bool speaker_tone_click()
{
    return start_mode(1, 0, 1);
}

bool speaker_tone_action()
{
    return start_mode(2, 0, 1);
}

bool speaker_tone_alarm(uint8_t intensity_level)
{
    return start_mode(3, intensity_level, 1);
}

bool speaker_tone_alarm_variant(uint8_t melody_id, uint8_t intensity_level)
{
    return start_mode(3, intensity_level, melody_id);
}

void speaker_tone_stop()
{
    s_tone_stop_requested = true;
}
