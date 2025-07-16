#include "audio.h"
#include <vector>
#include <cmath>
#include <atomic>
#include <SDL2/SDL.h>

constexpr int BEEP_FREQ = 440;
constexpr int BEEP_MS = 100;
constexpr int AUDIO_RATE = 48000;
constexpr int AUDIO_SAMPLES = AUDIO_RATE * BEEP_MS / 1000;
std::vector<float> beepBuffer(AUDIO_SAMPLES);
std::atomic<bool> beepRequested{false};

constexpr int BOOM_FREQ = 120;
constexpr int BOOM_MS = 250;
constexpr int BOOM_SAMPLES = AUDIO_RATE * BOOM_MS / 1000;
std::vector<float> boomBuffer(BOOM_SAMPLES);
std::atomic<bool> boomRequested{false};

constexpr int PEW_FREQ = 880;
constexpr int PEW_MS = 60;
constexpr int PEW_SAMPLES = AUDIO_RATE * PEW_MS / 1000;
std::vector<float> pewBuffer(PEW_SAMPLES);
std::atomic<bool> pewRequested{false};


void fillBeepBuffer() {
    for (int i = 0; i < AUDIO_SAMPLES; ++i) {
        float t = float(i) / AUDIO_RATE;
        beepBuffer[i] = 0.25f * sinf(2.0f * 3.1415926f * BEEP_FREQ * t);
    }
    for (int i = 0; i < PEW_SAMPLES; ++i) {
        float t = float(i) / AUDIO_RATE;
        pewBuffer[i] = 0.35f * sinf(2.0f * 3.1415926f * PEW_FREQ * t) * (1.0f - t * 1.2f);
    }
    for (int i = 0; i < BOOM_SAMPLES; ++i) {
        float t = float(i) / AUDIO_RATE;
        float decay = expf(-3.0f * t);
        float noise = ((rand() % 2000) / 1000.0f - 1.0f) * 0.2f;
        boomBuffer[i] = (0.5f * sinf(2.0f * 3.1415926f * BOOM_FREQ * t) + noise) * decay;
    }
}

void audioCallback(void* userdata, Uint8* stream, int len) {
    BeepState* state = (BeepState*)userdata;
    float* out = (float*)stream;
    int samples = len / sizeof(float);
    for (int i = 0; i < samples; ++i) {
        if (state->play == SoundType::Beep && state->beepPos < AUDIO_SAMPLES) {
            out[i] = beepBuffer[state->beepPos++];
        } else if (state->play == SoundType::Pew && state->pewPos < PEW_SAMPLES) {
            out[i] = pewBuffer[state->pewPos++];
        } else if (state->play == SoundType::Boom && state->boomPos < BOOM_SAMPLES) {
            out[i] = boomBuffer[state->boomPos++];
        } else {
            out[i] = 0.0f;
        }
    }
    if (beepRequested.exchange(false)) {
        state->beepPos = 0;
        state->play = SoundType::Beep;
    } else if (pewRequested.exchange(false)) {
        state->pewPos = 0;
        state->play = SoundType::Pew;
    } else if (boomRequested.exchange(false)) {
        state->boomPos = 0;
        state->play = SoundType::Boom;
    } else if ((state->play == SoundType::Beep && state->beepPos >= AUDIO_SAMPLES) || (state->play == SoundType::Pew && state->pewPos >= PEW_SAMPLES) || (state->play == SoundType::Boom && state->boomPos >= BOOM_SAMPLES)) {
        state->play = SoundType::None;
    }
}
