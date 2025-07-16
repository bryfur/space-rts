#pragma once

#include "../core/ECSRegistry.h"

/**
 * @brief Audio management system
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
    bool mInitialized;
    bool mMusicPlaying;
    float mMusicVolume;
};
