/**
 * minigb_apu is released under the terms listed within the LICENSE file.
 *
 * minigb_apu emulates the audio processing unit (APU) of the Game Boy. This
 * project is based on MiniGBS by Alex Baines: https://github.com/baines/MiniGBS
 */

#include "minigb_apu.h"

#include "../peanut_gb/peanut_gb.h"
#include "../src/game_scene.h"
#include "app.h"
#include "dtcm.h"
#include "preferences.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DMG_CLOCK_FREQ_U ((unsigned)DMG_CLOCK_FREQ)

#define AUDIO_MEM_SIZE (0xFF40 - 0xFF10)
#define AUDIO_ADDR_COMPENSATION 0xFF10

#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif

#ifndef MIN
#define MIN(a, b) (a <= b ? a : b)
#endif

#define VOL_INIT_MAX (INT16_MAX / 8)
#define VOL_INIT_MIN (INT16_MIN / 8)

/* Handles time keeping for sound generation.
 * This is a fixed reference to ensure timing calculations are consistent
 * regardless of the output sample rate. */
#define FREQ_INC_REF 44100

#define MAX_CHAN_VOLUME 15

#ifdef TARGET_SIMULATOR
#define __audio
#else
#define __audio \
    __attribute__((optimize("Os"))) __attribute__((section(".audio"))) __attribute__((short_call))
#endif

int audio_enabled;

#if !SDK_AUDIO

// =============================================================================
//
//   SOFTWARE APU EMULATION ONLY
//
// This entire block of code is for the software APU emulation only.
// It will be completely excluded from compilation when SDK_AUDIO is enabled.
//
// =============================================================================

#define audio_mem(audio) \
    ((uint8_t*)((void*)audio - offsetof(struct gb_s, audio) + offsetof(struct gb_s, hram) + 0x10))

/**
 * Memory holding audio registers between 0xFF10 and 0xFF3F inclusive.
 */
static uint32_t precomputed_noise_freqs[8][16];

static inline int get_sample_replication(void)
{
    // preferences_sample_rate: 0 -> 1 (44.1kHz), 1 -> 2 (22.05kHz)
    return preferences_sample_rate + 1;
}

static inline int get_audio_sample_rate(void)
{
    return FREQ_INC_REF / get_sample_replication();
}

__audio static void set_note_freq(struct chan* c, const uint32_t freq)
{
    /* Lowest expected value of freq is 64. */
    // Set frequency increment
    c->freq_inc = freq;
}

static void chan_enable(audio_data* audio, const uint_fast8_t i, const bool enable)
{
    uint8_t val;
    struct chan* chans = audio->chans;
    chans[i].enabled = enable;
    val = (audio_mem(audio)[0xFF26 - AUDIO_ADDR_COMPENSATION] & 0x80) | (chans[3].enabled << 3) |
          (chans[2].enabled << 2) | (chans[1].enabled << 1) | (chans[0].enabled << 0);

    audio_mem(audio)[0xFF26 - AUDIO_ADDR_COMPENSATION] = val;
}

__audio static void update_env(struct chan* c, int sample_rate)
{
    c->env.counter += c->env.inc;

    while (c->env.counter > sample_rate)
    {
        if (c->env.step)
        {
            c->volume += c->env.up ? 1 : -1;
            if (c->volume == 0 || c->volume == MAX_CHAN_VOLUME)
            {
                c->env.inc = 0;
            }
            c->volume = MAX(0, MIN(MAX_CHAN_VOLUME, c->volume));
        }
        c->env.counter -= sample_rate;
    }
}

// returns sample index at which to stop outputting in channel
__audio static int update_len(audio_data* restrict audio, struct chan* c, int len)
{
    if (!c->enabled)
        return 0;

    if (!c->len_enabled || c->len.inc == 0)
        return len;

    int sample_rate = get_audio_sample_rate();
    int tr = (sample_rate - c->len.counter) / c->len.inc;

    if (tr > len)
    {
        c->len.counter += len * c->len.inc;
        return len;
    }
    else
    {
        c->len.counter = 0;
        chan_enable(audio, c - audio->chans, 0);
        return tr;
    }
}

// This function is only for the "Accurate" mode.
__audio static bool update_freq(struct chan* c, uint32_t* pos, int sample_rate)
{
    uint32_t inc = c->freq_inc - *pos;
    c->freq_counter += inc;

    if (c->freq_counter > sample_rate)
    {
        *pos = c->freq_inc - (c->freq_counter - sample_rate);
        c->freq_counter = 0;
        return true;
    }
    else
    {
        *pos = c->freq_inc;
        return false;
    }
}

__audio static void update_sweep(struct chan* c, int sample_rate)
{
    c->sweep.counter += c->sweep.inc;

    while (c->sweep.counter > sample_rate)
    {
        if (c->sweep.shift)
        {
            uint16_t inc = (c->sweep.freq >> c->sweep.shift);
            if (!c->sweep_up)
                inc *= -1;

            c->freq += inc;
            if (c->freq > 2047)
            {
                c->enabled = 0;
            }
            else
            {
                set_note_freq(c, DMG_CLOCK_FREQ_U / ((2048 - c->freq) << 5));
                c->freq_inc *= 8;
            }
        }
        else if (c->sweep.rate)
        {
            c->enabled = 0;
        }
        c->sweep.counter -= sample_rate;
    }
}

__audio static void update_square(
    audio_data* restrict audio, int16_t* left, int16_t* right, const bool ch2, int len
)
{
    struct chan* c = audio->chans + ch2;

    if (!c->powered || !c->enabled)
        return;

    uint32_t freq = DMG_CLOCK_FREQ_U / ((2048 - c->freq) << 5);
    set_note_freq(c, freq);
    c->freq_inc *= 8;

    if (preferences_sound_mode != 2)
    {
        if (c->freq_inc == 0)
            return;
    }

    len = update_len(audio, c, len);
    int sample_replication = get_sample_replication();
    int sample_rate = get_audio_sample_rate();

    for (uint_fast16_t i = 0; i < len; i += sample_replication)
    {
        update_env(c, sample_rate);
        if (!ch2)
            update_sweep(c, sample_rate);

        if (preferences_sound_mode == 2)
        {
            uint32_t pos = 0;
            uint32_t prev_pos = 0;
            int32_t sample = 0;

            while (update_freq(c, &pos, sample_rate))
            {
                c->square.duty_counter = (c->square.duty_counter + 1) & 7;
                sample += ((pos - prev_pos) / c->freq_inc) * c->val;
                c->val = (c->square.duty & (1 << c->square.duty_counter))
                             ? VOL_INIT_MAX / MAX_CHAN_VOLUME
                             : VOL_INIT_MIN / MAX_CHAN_VOLUME;
                prev_pos = pos;
            }

            if (c->muted)
                continue;

            sample += c->val;
            sample *= c->volume;
            sample /= 4;

            left[i] += sample * c->on_left * audio->vol_l;
            right[i] += sample * c->on_right * audio->vol_r;
        }
        else
        {
            c->freq_counter += c->freq_inc;
            while (c->freq_counter >= sample_rate)
            {
                c->freq_counter -= sample_rate;
                c->square.duty_counter = (c->square.duty_counter + 1) & 7;
                c->val = (c->square.duty & (1 << c->square.duty_counter))
                             ? VOL_INIT_MAX / MAX_CHAN_VOLUME
                             : VOL_INIT_MIN / MAX_CHAN_VOLUME;
            }

            if (c->muted)
                continue;

            int32_t sample = c->val;
            sample *= c->volume;
            sample >>= 2;

            left[i] += sample * c->on_left * audio->vol_l;
            right[i] += sample * c->on_right * audio->vol_r;
        }
    }
}

__audio static uint8_t wave_sample(
    audio_data* audio, const unsigned int pos, const unsigned int volume
)
{
    uint8_t sample;

    sample = audio_mem(audio)[(0xFF30 + pos / 2) - AUDIO_ADDR_COMPENSATION];
    if (pos & 1)
    {
        sample &= 0xF;
    }
    else
    {
        sample >>= 4;
    }
    return volume ? (sample >> (volume - 1)) : 0;
}

__audio static void update_wave(audio_data* restrict audio, int16_t* left, int16_t* right, int len)
{
    struct chan* chans = audio->chans;
    struct chan* c = chans + 2;

    if (!c->powered || !c->enabled)
        return;

    uint32_t freq = (DMG_CLOCK_FREQ_U / 64) / (2048 - c->freq);
    set_note_freq(c, freq);
    c->freq_inc *= 32;

    if (preferences_sound_mode != 2)
    {
        if (c->freq_inc == 0)
            return;
    }

    len = update_len(audio, c, len);
    int sample_replication = get_sample_replication();
    int sample_rate = get_audio_sample_rate();

    for (uint_fast16_t i = 0; i < len; i += sample_replication)
    {
        if (preferences_sound_mode == 2)
        {
            uint32_t pos = 0;
            uint32_t prev_pos = 0;
            int32_t sample = 0;

            c->wave.sample = wave_sample(audio, c->val, c->volume);

            while (update_freq(c, &pos, sample_rate))
            {
                c->val = (c->val + 1) & 31;
                sample +=
                    ((pos - prev_pos) / c->freq_inc) * ((int)c->wave.sample - 8) * (INT16_MAX / 64);
                c->wave.sample = wave_sample(audio, c->val, c->volume);
                prev_pos = pos;
            }

            sample += ((int)c->wave.sample - 8) * (int)(INT16_MAX / 64);

            if (c->volume == 0 || c->muted)
                continue;

            sample /= 4;

            left[i] += sample * c->on_left * audio->vol_l;
            right[i] += sample * c->on_right * audio->vol_r;
        }
        else
        {
            c->freq_counter += c->freq_inc;
            while (c->freq_counter >= sample_rate)
            {
                c->freq_counter -= sample_rate;
                c->val = (c->val + 1) & 31;
            }

            if (c->volume == 0 || c->muted)
                continue;

            uint8_t wave_val = wave_sample(audio, c->val, c->volume);
            int32_t sample = ((int)wave_val - 8) * (INT16_MAX / 64);

            sample >>= 2;

            left[i] += sample * c->on_left * audio->vol_l;
            right[i] += sample * c->on_right * audio->vol_r;
        }
    }
}

__audio static void update_noise(audio_data* restrict audio, int16_t* left, int16_t* right, int len)
{
    struct chan* c = audio->chans + 3;

    if (!c->powered)
        return;
    {
        uint32_t freq = precomputed_noise_freqs[c->noise.lfsr_div][c->freq];
        set_note_freq(c, freq);

        // A frequency of 0 would cause a division by zero in accurate sound
        // mode.
        if (c->freq_inc == 0)
            return;
    }

    if (c->freq >= 14)
        c->enabled = 0;

    len = update_len(audio, c, len);

    if (!c->enabled)
        return;

    int sample_replication = get_sample_replication();
    int sample_rate = get_audio_sample_rate();
    for (uint_fast16_t i = 0; i < len; i += sample_replication)
    {
        update_env(c, sample_rate);

        if (preferences_sound_mode == 2)
        {
            uint32_t pos = 0;
            uint32_t prev_pos = 0;
            int32_t sample = 0;

            while (update_freq(c, &pos, sample_rate))
            {
                c->noise.lfsr_reg =
                    (c->noise.lfsr_reg << 1) | (c->val >= VOL_INIT_MAX / MAX_CHAN_VOLUME);

                if (c->lfsr_wide)
                {
                    c->val = !(((c->noise.lfsr_reg >> 14) & 1) ^ ((c->noise.lfsr_reg >> 13) & 1))
                                 ? VOL_INIT_MAX / MAX_CHAN_VOLUME
                                 : VOL_INIT_MIN / MAX_CHAN_VOLUME;
                }
                else
                {
                    c->val = !(((c->noise.lfsr_reg >> 6) & 1) ^ ((c->noise.lfsr_reg >> 5) & 1))
                                 ? VOL_INIT_MAX / MAX_CHAN_VOLUME
                                 : VOL_INIT_MIN / MAX_CHAN_VOLUME;
                }
                sample += ((pos - prev_pos) / c->freq_inc) * c->val;
                prev_pos = pos;
            }

            if (c->muted)
                continue;

            sample += c->val;
            sample *= c->volume;
            sample /= 4;

            left[i] += sample * c->on_left * audio->vol_l;
            right[i] += sample * c->on_right * audio->vol_r;
        }
        else
        {
            c->freq_counter += c->freq_inc;
            while (c->freq_counter >= sample_rate)
            {
                c->freq_counter -= sample_rate;

                uint16_t old_lfsr = c->noise.lfsr_reg;
                c->noise.lfsr_reg <<= 1;

                uint8_t xor_res = (c->lfsr_wide) ? (((old_lfsr >> 14) & 1) ^ ((old_lfsr >> 13) & 1))
                                                 : (((old_lfsr >> 6) & 1) ^ ((old_lfsr >> 5) & 1));

                c->noise.lfsr_reg |= xor_res;
                c->val = !xor_res ? VOL_INIT_MAX / MAX_CHAN_VOLUME : VOL_INIT_MIN / MAX_CHAN_VOLUME;
            }

            if (c->muted)
                continue;

            int32_t sample = c->val;
            sample *= c->volume;
            sample >>= 2;

            left[i] += sample * c->on_left * audio->vol_l;
            right[i] += sample * c->on_right * audio->vol_r;
        }
    }
}

static void chan_trigger(audio_data* restrict audio, uint_fast8_t i)
{
    struct chan* chans = audio->chans;
    struct chan* c = chans + i;

    chan_enable(audio, i, 1);
    c->volume = c->volume_init;

    // volume envelope
    {
        uint8_t val = audio_mem(audio)[(0xFF12 + (i * 5)) - AUDIO_ADDR_COMPENSATION];

        c->env.step = val & 0x07;
        c->env.up = val & 0x08 ? 1 : 0;
        c->env.inc = c->env.step ? 64ul / (uint32_t)c->env.step : 8ul;
        c->env.counter = 0;
    }

    // freq sweep
    if (i == 0)
    {
        uint8_t val = audio_mem(audio)[0xFF10 - AUDIO_ADDR_COMPENSATION];

        c->sweep.freq = c->freq;
        c->sweep.rate = (val >> 4) & 0x07;
        c->sweep_up = !(val & 0x08);
        c->sweep.shift = (val & 0x07);
        c->sweep.inc = c->sweep.rate ? 128 / c->sweep.rate : 0;
        c->sweep.counter = get_audio_sample_rate();
    }

    int len_max = 64;

    if (i == 2)
    {  // wave
        len_max = 256;
        c->val = 0;
    }
    else if (i == 3)
    {  // noise
        c->noise.lfsr_reg = 0xFFFF;
        c->val = VOL_INIT_MIN / MAX_CHAN_VOLUME;
    }

    c->len.inc = (len_max > c->len.load) ? 256 / (len_max - c->len.load) : 0;
    c->len.counter = 0;
}

#endif  // !SDK_AUDIO

// =============================================================================
//
//   PUBLIC AUDIO FUNCTIONS
//
// =============================================================================

#if SDK_AUDIO
void free_transient_sample_callback(SoundSource* source, void* userdata)
{
    AudioSample* sample_to_free = (AudioSample*)userdata;
    if (sample_to_free)
    {
        playdate->sound->sample->freeSample(sample_to_free);
    }
}

void sdk_trigger_channel(struct gb_s* gb, int i)
{
    sdk_audio_data* sdk_audio = &gb->sdk_audio;
    sdk_channel_state* channel = &sdk_audio->channels[i];

    // --- DAC Power Check (For Wave Channel) ---
    if (i == 2)
    {
        uint8_t nr30 = gb->hram[0xFF1A - 0xFF00];
        if (!(nr30 & 0x80))
        {
            playdate->sound->synth->noteOff(sdk_audio->synth[2], 0);
            channel->note_is_on = false;
            return;
        }
    }

    // Reconstruct the 11-bit frequency value
    uint8_t freq_lo = gb->hram[(0xFF13 + (i * 5)) - 0xFF00];
    uint8_t freq_hi_byte = gb->hram[(0xFF14 + (i * 5)) - 0xFF00];
    uint16_t gb_freq = ((freq_hi_byte & 0x07) << 8) | freq_lo;

    // --- Per-channel trigger logic ---
    if (i == 2)  // Wave Channel
    {
        // --- Step 1: Stop any currently playing note. ---
        // This is the crucial step. It ensures the finish callback for the *previous*
        // sample is triggered, preventing a memory leak and race conditions.
        playdate->sound->synth->noteOff(sdk_audio->synth[2], 0);

        // --- Step 2: Calculate volume and frequency parameters (same as before) ---
        uint8_t volume_code = (gb->hram[0xFF1C - 0xFF00] >> 5) & 0x03;
        float initial_volume = 0.0f;
        switch (volume_code)
        {
        case 0:
            initial_volume = 0.0f;
            break;
        case 1:
            initial_volume = 1.0f;
            break;
        case 2:
            initial_volume = 0.5f;
            break;
        case 3:
            initial_volume = 0.25f;
            break;
        }

        float cycle_freq_hz = 65536.0f / (2048.0f - gb_freq);
        if (gb_freq >= 2048)
        {  // Prevent division by zero or negative rates
            return;
        }
        float playback_rate_hz = cycle_freq_hz * 32.0f;

        // DEBUG LOGS
#if WAVE_CHANNEL_DEBUG > 0
        playdate->system->logToConsole("--- Wave Channel Trigger ---");
        uint8_t nr30 = gb->hram[0xFF1A - 0xFF00];
        uint8_t nr32 = gb->hram[0xFF1C - 0xFF00];
        playdate->system->logToConsole(
            "NR30 (DAC Power): 0x%02X, NR32 (Volume): 0x%02X, GB Freq: %d", nr30, nr32, gb_freq
        );
#endif

        // --- Step 3: Create the new sample data (same as before) ---
        uint8_t* wave_audio_data = playdate->system->realloc(NULL, 32);
        if (!wave_audio_data)
        {
            playdate->system->error("Waveform malloc failed");
            return;
        }
        for (int j = 0; j < 16; j++)
        {
            uint8_t wave_byte = gb->hram[(0xFF30 + j) - 0xFF00];
            wave_audio_data[j * 2] = (((wave_byte >> 4)) - 8) * 16 + 128;
            wave_audio_data[j * 2 + 1] = (((wave_byte & 0x0F)) - 8) * 16 + 128;
        }

        // --- Step 4: Create and play the new note using the callback method ---
        AudioSample* sample = playdate->sound->sample->newSampleFromData(
            wave_audio_data, kSound8bitMono, playback_rate_hz, 32, 1  // 1 = free data with sample
        );

        playdate->sound->synth->setSample(sdk_audio->synth[2], sample, 0, 31);

        // Set the callback to safely free the sample when the note is done.
        // The sample itself is passed as the userdata to be freed.
        playdate->sound->source->setFinishCallback(
            (SoundSource*)sdk_audio->synth[2], free_transient_sample_callback, sample
        );

        // Play with full velocity, then set the actual volume.
        playdate->sound->synth->playNote(sdk_audio->synth[2], 1.0f, 1.0f, -1, 0);
        playdate->sound->synth->setVolume(sdk_audio->synth[2], initial_volume, initial_volume);

#if WAVE_CHANNEL_DEBUG > 0
        char wave_ram_str[128] = "Wave RAM: ";
        for (int k = 0; k < 16; ++k)
        {
            char byte_str[6];
            snprintf(byte_str, sizeof(byte_str), " %02X", gb->hram[(0xFF30 + k) - 0xFF00]);
            strcat(wave_ram_str, byte_str);
        }
        playdate->system->logToConsole(wave_ram_str);
#endif

#if WAVE_CHANNEL_DEBUG > 0
        playdate->system->logToConsole(
            "Volume: %.2f, Playback Rate: %.2f Hz", (double)initial_volume, (double)playback_rate_hz
        );
#endif
    }
    else  // --- Logic for Square and Noise Channels ---
    {
        float freq_hz;
        float initial_volume;

        if (i == 3)  // Noise Channel
        {
            uint8_t nr42 = gb->hram[0xFF21 - 0xFF00];
            channel->current_volume_step = (nr42 >> 4);
            initial_volume = channel->current_volume_step / 15.0f;

            uint8_t nr43 = gb->hram[0xFF22 - 0xFF00];
            const int divisors[] = {8, 16, 32, 48, 64, 80, 96, 112};
            int clock_shift = nr43 >> 4;
            if (clock_shift > 13)
            {
                channel->note_is_on = false;
                return;
            }
            freq_hz = (DMG_CLOCK_FREQ / divisors[nr43 & 0x07]) / (1 << clock_shift);
        }
        else  // Square Channels
        {
            uint8_t nrX2 = gb->hram[(0xFF12 + (i * 5)) - 0xFF00];
            channel->current_volume_step = (nrX2 >> 4);
            initial_volume = channel->current_volume_step / 15.0f;

            freq_hz = 131072.0f / (2048.0f - gb_freq);
        }

        if (i == 0)  // Special Sweep setup for Channel 1
        {
            uint8_t nr10 = gb->hram[0xFF10 - 0xFF00];
            sdk_audio->sweep_state.period = (nr10 >> 4) & 0x07;
            sdk_audio->sweep_state.negate = (nr10 & 0x08) != 0;
            sdk_audio->sweep_state.shift = nr10 & 0x07;
            sdk_audio->sweep_state.shadow_freq = gb_freq;
            sdk_audio->sweep_state.timer = 0.0f;

            if (sdk_audio->sweep_state.period > 0 && sdk_audio->sweep_state.shift > 0)
            {
                uint16_t new_freq = sdk_audio->sweep_state.negate
                                        ? (gb_freq - (gb_freq >> sdk_audio->sweep_state.shift))
                                        : (gb_freq + (gb_freq >> sdk_audio->sweep_state.shift));
                if (new_freq > 2047)
                {
                    channel->note_is_on = false;
                    return;
                }
            }
        }
        playdate->sound->synth->playNote(sdk_audio->synth[i], freq_hz, initial_volume, -1, 0);
    }

    // --- Common Post-Trigger Logic for All Channels ---
    channel->note_is_on = true;

    // Set the synth's master volume to full. Subsequent envelope changes will modulate this.
    playdate->sound->synth->setVolume(sdk_audio->synth[i], 1.0f, 1.0f);

    // Volume Envelope Initialization (Channels 0, 1, 3)
    if (i != 2)
    {
        uint8_t nrX2_addr_offset = (i == 3) ? (0xFF21 - 0xFF10) : (0xFF12 + (i * 5) - 0xFF10);
        uint8_t nrX2 = gb->hram[(0xFF10 + nrX2_addr_offset) - 0xFF00];

        uint8_t envelope_sweep_num = nrX2 & 0x07;
        if (envelope_sweep_num == 0)
        {
            channel->envelope_period = 0.0f;
        }
        else
        {
            channel->envelope_period = envelope_sweep_num * (1.0f / 64.0f);
            channel->envelope_direction = (nrX2 & 0x08) ? 1 : -1;
        }
        channel->envelope_timer = 0.0f;
    }

    // Length Counter Initialization (All Channels)
    uint8_t nrX4_addr_offset = (i == 3) ? (0xFF23 - 0xFF10) : (0xFF14 + (i * 5) - 0xFF10);
    uint8_t nrX4 = gb->hram[(0xFF10 + nrX4_addr_offset) - 0xFF00];

    if (nrX4 & 0x40)
    {  // Length counter is enabled
        uint8_t nrX1 = 0;
        if (i == 0)
            nrX1 = gb->hram[0xFF11 - 0xFF00];
        else if (i == 1)
            nrX1 = gb->hram[0xFF16 - 0xFF00];
        else if (i == 2)
            nrX1 = gb->hram[0xFF1B - 0xFF00];
        else if (i == 3)
            nrX1 = gb->hram[0xFF20 - 0xFF00];

        int max_len = (i == 2) ? 256 : 64;
        int t1 = nrX1 & (max_len - 1);
        float duration_s = (max_len - t1) * (1.0f / 256.0f);
        channel->length_timer = duration_s;
    }
    else
    {
        channel->length_timer = -1.0f;
    }
}
#endif

/**
 * Read audio register.
 * \param addr  Address of audio register. Must be 0xFF10 <= addr <= 0xFF3F.
 *              This is not checked in this function.
 * \return      Byte at address.
 */
uint8_t audio_read(audio_data* audio, const uint16_t addr)
{
#if SDK_AUDIO
    return 0xFF;
#else
    /* clang-format off */
    static const uint8_t ortab[] =
    {
        0x80, 0x3f, 0x00, 0xff, 0xbf,
        0xff, 0x3f, 0x00, 0xff, 0xbf,
        0x7f, 0xff, 0x9f, 0xff, 0xbf,
        0xff, 0xff, 0x00, 0x00, 0xbf,
        0x00, 0x00, 0x70,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    /* clang-format on */

    return audio_mem(audio)[addr - AUDIO_ADDR_COMPENSATION] | ortab[addr - AUDIO_ADDR_COMPENSATION];
#endif
}

/**
 * Write audio register.
 * \param addr  Address of audio register. Must be 0xFF10 <= addr <= 0xFF3F.
 *              This is not checked in this function.
 * \param val   Byte to write at address.
 */
void audio_write(audio_data* restrict audio, const uint16_t addr, const uint8_t val)
{
#if SDK_AUDIO
    sdk_audio_data* sdk_audio = (sdk_audio_data*)audio;
    struct gb_s* gb = (struct gb_s*)((uint8_t*)sdk_audio - offsetof(struct gb_s, sdk_audio));

    if (addr >= 0xFF10 && addr <= 0xFF3F)
    {
        gb->hram[addr - 0xFF00] = val;
    }

    // Determine channel index for most registers.
    int i = (addr - 0xFF10) / 5;

    switch (addr)
    {
    // --- Duty Cycle for Square Waves (Channels 0 & 1) ---
    case 0xFF11:  // NR11
    case 0xFF16:  // NR21
    {
        if (i > 1)
            break;  // Should not happen, but as a safeguard.
        float duty = 0.5f;
        switch (val >> 6)
        {
        case 0:
            duty = 0.125f;
            break;
        case 1:
            duty = 0.25f;
            break;
        case 2:
            duty = 0.50f;
            break;
        case 3:
            duty = 0.75f;
            break;
        }
        playdate->sound->synth->setParameter(sdk_audio->synth[i], 1, duty);
        break;
    }

    // --- Volume & Envelope for Square/Noise (Channels 0, 1, 3) ---
    case 0xFF12:  // NR12
    case 0xFF17:  // NR22
    case 0xFF21:  // NR42
    {
        // For NR42, i will be 2 from division, but we need channel 3 (noise).
        int chan_idx = (addr == 0xFF21) ? 3 : i;
        sdk_channel_state* channel = &sdk_audio->channels[chan_idx];

        // Update our internal envelope state from the register value.
        channel->current_volume_step = (val >> 4);
        uint8_t envelope_steps = val & 0x07;

        if (envelope_steps == 0)
        {
            channel->envelope_period = 0.0f;  // Envelope disabled.
        }
        else
        {
            channel->envelope_period = envelope_steps * (1.0f / 64.0f);
            channel->envelope_direction = (val & 0x08) ? 1 : -1;
        }
        channel->envelope_timer = 0.0f;  // Reset timer to react immediately.

        // If a note is currently playing, apply the new volume right away.
        if (channel->note_is_on)
        {
            float new_volume = channel->current_volume_step / 15.0f;
            playdate->sound->synth->setVolume(sdk_audio->synth[chan_idx], new_volume, new_volume);
        }
        break;
    }

    // --- Volume for Wave Channel (Channel 2) ---
    case 0xFF1C:  // NR32
    {
        uint8_t volume_code = (val >> 5) & 0x03;
        float volume_f = 0.0f;
        switch (volume_code)
        {
        case 0:
            volume_f = 0.0f;
            break;  // Muted
        case 1:
            volume_f = 1.0f;
            break;  // 100%
        case 2:
            volume_f = 0.5f;
            break;  // 50%
        case 3:
            volume_f = 0.25f;
            break;  // 25%
        }

#if WAVE_CHANNEL_DEBUG > 0
        playdate->system->logToConsole(
            "WAVE VOLUME WRITE: NR32=0x%02X, new_volume=%.2f", val, (double)volume_f
        );
#endif

        // Simply set the synth's volume. The SDK handles the change smoothly.
        playdate->sound->synth->setVolume(sdk_audio->synth[2], volume_f, volume_f);
        break;
    }

    case 0xFF1A:  // NR30
    {
        bool dac_is_on = (val & 0x80) != 0;
        if (!dac_is_on)
        {
            // If the DAC is being turned off, stop any playing note on the
            // synth.
            playdate->sound->synth->noteOff(sdk_audio->synth[2], 0);
            sdk_audio->channels[2].note_is_on = false;
        }
        break;
    }

    // --- Trigger Events & Length Counter Control ---
    case 0xFF14:  // NR14
    case 0xFF19:  // NR24
    case 0xFF1E:  // NR34
    case 0xFF23:  // NR44
    {
        int chan_idx = (addr == 0xFF23) ? 3 : i;
        sdk_channel_state* channel = &sdk_audio->channels[chan_idx];

        // Bit 7 is the trigger bit. If set, start a new note.
        if (val & 0x80)
        {
            sdk_trigger_channel(gb, chan_idx);
        }
        // If trigger bit is NOT set, a game might be toggling the length
        // counter mid-note.
        else
        {
            bool length_enabled = val & 0x40;
            // If length is now enabled and was previously disabled.
            if (length_enabled && channel->length_timer < 0)
            {
                // Re-initialize the length timer as if the note just started.
                uint8_t nrX1 = gb->hram[(0xFF11 + (chan_idx * 5)) - 0xFF00];
                int max_len = (chan_idx == 2) ? 256 : 64;
                int t1 = nrX1 & (max_len - 1);
                channel->length_timer = (max_len - t1) * (1.0f / 256.0f);
            }
            // If length is now disabled.
            else if (!length_enabled)
            {
                channel->length_timer = -1.0f;  // A negative value signals infinite duration.
            }
        }
        break;
    }
    }
#else
    /* Find sound channel corresponding to register address. */
    uint_fast8_t i;
    struct chan* chans = audio->chans;

    if (addr == 0xFF26)
    {
        audio_mem(audio)[addr - AUDIO_ADDR_COMPENSATION] = val & 0x80;
        /* On APU power off, clear all registers apart from wave RAM. */
        if ((val & 0x80) == 0)
        {
            memset(audio_mem(audio), 0x00, 0xFF26 - AUDIO_ADDR_COMPENSATION);
            chans[0].enabled = false;
            chans[1].enabled = false;
            chans[2].enabled = false;
            chans[3].enabled = false;
        }
        return;
    }

    /* Ignore register writes if APU powered off. */
    if (audio_mem(audio)[0xFF26 - AUDIO_ADDR_COMPENSATION] == 0x00)
        return;

    audio_mem(audio)[addr - AUDIO_ADDR_COMPENSATION] = val;

    if (preferences_sound_mode == 2)
    {
        i = (addr - AUDIO_ADDR_COMPENSATION) * 0.2f;
    }
    else
    {
        i = (addr - AUDIO_ADDR_COMPENSATION) / 5;
    }

    switch (addr)
    {
    case 0xFF12:
    case 0xFF17:
    case 0xFF21:
    {
        chans[i].volume_init = val >> 4;
        chans[i].powered = (val >> 3) != 0;

        if (chans[i].powered && chans[i].enabled)
        {
            if ((chans[i].env.step == 0 && chans[i].env.inc != 0))
            {
                if (val & 0x08)
                {
                    chans[i].volume++;
                }
                else
                {
                    chans[i].volume += 2;
                }
            }
            else
            {
                chans[i].volume = 16 - chans[i].volume;
            }
            chans[i].volume &= 0x0F;
            chans[i].env.step = val & 0x07;
        }
    }
    break;

    case 0xFF1C:
        chans[i].volume = chans[i].volume_init = (val >> 5) & 0x03;
        break;

    case 0xFF11:
    case 0xFF16:
    case 0xFF20:
    {
        static const uint8_t duty_lookup[] = {0x10, 0x30, 0x3C, 0xCF};
        chans[i].len.load = val & 0x3f;
        if (i < 2)
        {  // Only for square channels
            chans[i].square.duty = duty_lookup[val >> 6];
        }
        break;
    }

    case 0xFF1B:
        chans[i].len.load = val;
        break;

    case 0xFF13:
    case 0xFF18:
    case 0xFF1D:
        chans[i].freq &= 0xFF00;
        chans[i].freq |= val;
        break;

    case 0xFF1A:
        chans[i].powered = (val & 0x80) != 0;
        chan_enable(audio, i, val & 0x80);
        break;

    case 0xFF14:
    case 0xFF19:
    case 0xFF1E:
        chans[i].freq &= 0x00FF;
        chans[i].freq |= ((val & 0x07) << 8);
        /* Intentional fall-through. */
    case 0xFF23:
        chans[i].len_enabled = val & 0x40 ? 1 : 0;
        if (val & 0x80)
            chan_trigger(audio, i);
        break;

    case 0xFF22:
        chans[3].freq = val >> 4;
        chans[3].lfsr_wide = !(val & 0x08);
        chans[3].noise.lfsr_div = val & 0x07;
        break;

    case 0xFF24:
        audio->vol_l = ((val >> 4) & 0x07);
        audio->vol_r = (val & 0x07);
        break;

    case 0xFF25:
        for (uint_fast8_t j = 0; j < 4; j++)
        {
            chans[j].on_left = (val >> (4 + j)) & 1;
            chans[j].on_right = (val >> j) & 1;
        }
        break;
    }
#endif
}

void audio_init(audio_data* audio)
{
    /* Initialise channels and samples. */
#if SDK_AUDIO
    sdk_audio_data* sdk_audio = (sdk_audio_data*)audio;
    memset(sdk_audio, 0, sizeof(sdk_audio_data));

    for (int i = 0; i < 4; ++i)
    {
        sdk_audio->synth[i] = playdate->sound->synth->newSynth();
    }
    playdate->sound->synth->setWaveform(sdk_audio->synth[0], kWaveformSquare);
    playdate->sound->synth->setWaveform(sdk_audio->synth[1], kWaveformSquare);
    playdate->sound->synth->setWaveform(sdk_audio->synth[3], kWaveformNoise);

    // --- Wave Channel Synth Pre-configuration ---
    // The synth needs to be told it's a sample player. We do this by setting
    // a sample on it. It doesn't matter what the sample is, this just sets the mode.
    // We'll create a single silent sample here and use it for this purpose.
    uint8_t silent_data[1] = {128};
    AudioSample* initial_sample =
        playdate->sound->sample->newSampleFromData(silent_data, kSound8bitMono, 44100, 1, 0);
    if (initial_sample)
    {
        playdate->sound->synth->setSample(sdk_audio->synth[2], initial_sample, 0, 0);
        playdate->sound->sample->freeSample(initial_sample);
    }
#else
    struct chan* chans = audio->chans;
    memset(chans, 0, 4 * sizeof(struct chan));
    chans[0].val = chans[1].val = -1;

    /* Initialise IO registers. */
    { /* clang-format off */
        static const uint8_t regs_init[] = {
            0x80, 0xBF, 0xF3, 0xFF, 0x3F,
            0xFF, 0x3F, 0x00, 0xFF, 0x3F,
            0x7F, 0xFF, 0x9F, 0xFF, 0x3F,
            0xFF, 0xFF, 0x00, 0x00, 0x3F,
            0x77, 0xF3, 0xF1
        };
        /* clang-format on */

        for (uint_fast8_t i = 0; i < sizeof(regs_init); ++i)
            audio_write(audio, 0xFF10 + i, regs_init[i]);
    }

    /* Initialise Wave Pattern RAM. */
    { /* clang-format off */
        static const uint8_t wave_init[] = {
            0xac, 0xdd, 0xda, 0x48,
            0x36, 0x02, 0xcf, 0x16,
            0x2c, 0x04, 0xe5, 0x2c,
            0xac, 0xdd, 0xda, 0x48
        };
        /* clang-format on */

        for (uint_fast8_t i = 0; i < sizeof(wave_init); ++i)
            audio_write(audio, 0xFF30 + i, wave_init[i]);
    }

    for (uint8_t lfsr_selector_idx = 0; lfsr_selector_idx < 8; ++lfsr_selector_idx)
    {
        uint32_t current_lfsr_div_val = lfsr_selector_idx == 0 ? 8 : lfsr_selector_idx * 16;
        for (uint8_t c_freq_shift_val = 0; c_freq_shift_val < 16; ++c_freq_shift_val)
        {
            uint32_t divisor_term = current_lfsr_div_val << c_freq_shift_val;

            if (divisor_term == 0)
            {
                // This should ideally not happen with current_lfsr_div_val and
                // 0-15 shift
                precomputed_noise_freqs[lfsr_selector_idx][c_freq_shift_val] = 0;
            }
            else
            {
                precomputed_noise_freqs[lfsr_selector_idx][c_freq_shift_val] =
                    DMG_CLOCK_FREQ_U / divisor_term;
            }
        }
    }
#endif
}

/**
 * Playdate audio callback function.
 */
__audio int audio_callback(void* context, int16_t* left, int16_t* right, int len)
{
#if SDK_AUDIO
    return 0;
#else
    if (!audio_enabled)
        return 0;

    DTCM_VERIFY_DEBUG();

    PGB_GameScene** gameScene_ptr = context;
    PGB_GameScene* gameScene = *gameScene_ptr;

    if (!gameScene)
    {
        return 0;
    }

    if (gameScene->audioLocked)
    {
        return 0;
    }

#ifdef TARGET_SIMULATOR
    pthread_mutex_lock(&audio_mutex);
#endif

    audio_data* audio = &gameScene->context->gb->audio;

    __builtin_prefetch(left, 1);
    __builtin_prefetch(right, 1);

    int sample_replication = get_sample_replication();
    int max_chunk = ((256 + sample_replication - 1) / sample_replication) * sample_replication;

    while (len > 0)
    {
        int chunksize = len >= max_chunk ? max_chunk : len;

        memset(left, 0, chunksize * sizeof(int16_t));
        memset(right, 0, chunksize * sizeof(int16_t));

        update_wave(audio, left, right, chunksize);
        update_square(audio, left, right, 0, chunksize);
        update_square(audio, left, right, 1, chunksize);
        update_noise(audio, left, right, chunksize);

        if (sample_replication > 1)
        {
            for (int i = 0; i < chunksize; i += sample_replication)
            {
                for (int j = 1; j < sample_replication && (i + j) < chunksize; ++j)
                {
                    left[i + j] = left[i];
                    right[i + j] = right[i];
                }
            }
        }

        len -= chunksize;
        left += chunksize;
        right += chunksize;
    }

#ifdef TARGET_SIMULATOR
    pthread_mutex_unlock(&audio_mutex);
#endif

    DTCM_VERIFY_DEBUG();

    return 1;
#endif
}
