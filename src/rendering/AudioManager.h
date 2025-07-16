#pragma once

#include "../core/ECSRegistry.h"
#include <SDL2/SDL.h>
#include <vector>
#include <memory>

/**
 * @brief Audio management system with procedural sound generation
 */
class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    bool Initialize();
    void Update(float deltaTime);
    void Shutdown();

    // Audio events
    void PlayBeep();
    void PlayPew();
    void PlayBoom();
    
    // Background music
    void PlayBackgroundMusic();
    void StopBackgroundMusic();
    void SetMusicVolume(float volume); // 0.0 to 1.0

private:
    // Audio device management
    bool mInitialized;
    SDL_AudioDeviceID mAudioDevice;
    SDL_AudioSpec mAudioSpec;
    
    // Music system
    bool mMusicPlaying;
    float mMusicVolume;
    float mMusicTime;
    int mCurrentNote;
    
    // Audio generation
    struct ToneData {
        float frequency;
        float duration;
        float amplitude;
        float phase;
        bool active;
    };
    
    std::vector<ToneData> mActiveTones;
    
    // Audio callback and generation functions
    static void AudioCallback(void* userdata, Uint8* stream, int len);
    void GenerateAudio(Sint16* buffer, int samples);
    void PlayTone(float frequency, float duration, float amplitude = 0.3f);
    void GenerateBackgroundMusic(Sint16* buffer, int samples);
    
    // Sound effect generators
    void GenerateBeep(Sint16* buffer, int samples, float& phase);
    void GeneratePew(Sint16* buffer, int samples, float& phase);
    void GenerateBoom(Sint16* buffer, int samples, float& phase);
    
    // Music constants
    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int CHANNELS = 1;
    static constexpr int SAMPLES = 512;
};
