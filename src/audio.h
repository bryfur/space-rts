enum class SoundType { None, Beep, Pew, Boom };
#include <vector>
struct BeepState {
    std::vector<int> beepPositions;
    std::vector<int> pewPositions;
    std::vector<int> boomPositions;
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
