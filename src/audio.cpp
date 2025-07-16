#include "audio.h"
#include <vector>
#include <algorithm>
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
        float sample = 0.0f;
        // Mix all active beep instances
        for (size_t j = 0; j < state->beepPositions.size(); ++j) {
            if (state->beepPositions[j] < AUDIO_SAMPLES)
                sample += beepBuffer[state->beepPositions[j]++];
        }
        // Mix all active pew instances
        for (size_t j = 0; j < state->pewPositions.size(); ++j) {
            if (state->pewPositions[j] < PEW_SAMPLES)
                sample += pewBuffer[state->pewPositions[j]++];
        }
        // Mix all active boom instances
        for (size_t j = 0; j < state->boomPositions.size(); ++j) {
            if (state->boomPositions[j] < BOOM_SAMPLES)
                sample += boomBuffer[state->boomPositions[j]++];
        }
        out[i] = sample;
    }
    // Remove finished beep instances
    state->beepPositions.erase(
        std::remove_if(state->beepPositions.begin(), state->beepPositions.end(), [](int pos) { return pos >= AUDIO_SAMPLES; }),
        state->beepPositions.end());
    // Remove finished pew instances
    state->pewPositions.erase(
        std::remove_if(state->pewPositions.begin(), state->pewPositions.end(), [](int pos) { return pos >= PEW_SAMPLES; }),
        state->pewPositions.end());
    // Remove finished boom instances
    state->boomPositions.erase(
        std::remove_if(state->boomPositions.begin(), state->boomPositions.end(), [](int pos) { return pos >= BOOM_SAMPLES; }),
        state->boomPositions.end());
}
