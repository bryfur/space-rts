enum class SoundType { None, Beep, Pew, Boom };
struct BeepState {
    int beepPos;
    int pewPos;
    int boomPos;
    SoundType play;
};
#pragma once
#include <atomic>

void fillBeepBuffer();
// SDL defines Uint8 in SDL2/SDL.h
#include <SDL2/SDL.h>
void audioCallback(void* userdata, Uint8* stream, int len);
extern std::atomic<bool> beepRequested;
extern std::atomic<bool> pewRequested;
extern std::atomic<bool> boomRequested;
