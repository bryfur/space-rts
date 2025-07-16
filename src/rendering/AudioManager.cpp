#include "AudioManager.h"
#include <SDL_log.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

AudioManager::AudioManager()
    : mInitialized(false)
    , mAudioDevice(0)
    , mMusicPlaying(false)
    , mMusicVolume(0.5F)
    , mMusicTime(0.0F)
    , mCurrentNote(0)
{
}

AudioManager::~AudioManager() {
    Shutdown();
}

bool AudioManager::Initialize() {
    // Initialize SDL Audio subsystem
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        SDL_Log("Failed to initialize SDL Audio: %s", SDL_GetError());
        return false;
    }
    
    // Set up audio specification
    SDL_AudioSpec desired;
    desired.freq = SAMPLE_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = CHANNELS;
    desired.samples = SAMPLES;
    desired.callback = AudioCallback;
    desired.userdata = this;
    
    // Open audio device
    mAudioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &mAudioSpec, 0);
    if (mAudioDevice == 0) {
        SDL_Log("Failed to open audio device: %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }
    
    // Start audio playback
    SDL_PauseAudioDevice(mAudioDevice, 0);
    
    SDL_Log("Audio manager initialized - Sample Rate: %d, Channels: %d", 
            mAudioSpec.freq, mAudioSpec.channels);
    mInitialized = true;
    return true;
}

void AudioManager::Update(float deltaTime) {
    if (!mInitialized) {
        return;
    }
    
    // Update music timing
    if (mMusicPlaying) {
        mMusicTime += deltaTime;
    }
    
    // Clean up finished tones
    mActiveTones.erase(
        std::remove_if(mActiveTones.begin(), mActiveTones.end(),
            [](const ToneData& tone) { return !tone.active; }),
        mActiveTones.end());
}

void AudioManager::Shutdown() {
    if (mInitialized) {
        SDL_CloseAudioDevice(mAudioDevice);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_Log("Audio manager shutdown");
        mInitialized = false;
    }
}

void AudioManager::PlayBeep() {
    PlayTone(800.0F, 0.1F, 0.4F); // High-pitched short beep
}

void AudioManager::PlayPew() {
    PlayTone(220.0F, 0.15F, 0.3F); // Lower laser sound
}

void AudioManager::PlayBoom() {
    // Create explosion sound with multiple frequency components
    // Reduced amplitudes to prevent interference and clipping
    PlayTone(60.0F, 0.3F, 0.2F);   // Low rumble - reduced amplitude
    PlayTone(120.0F, 0.2F, 0.15F); // Mid frequency - reduced amplitude
    PlayTone(200.0F, 0.1F, 0.1F);  // High frequency crack - reduced amplitude
}

void AudioManager::PlayBackgroundMusic() {
    if (!mMusicPlaying) {
        mMusicPlaying = true;
        mMusicTime = 0.0F;
        mCurrentNote = 0;
        SDL_Log("Background music started");
    }
}

void AudioManager::StopBackgroundMusic() {
    if (mMusicPlaying) {
        mMusicPlaying = false;
        SDL_Log("Background music stopped");
    }
}

void AudioManager::SetMusicVolume(float volume) {
    mMusicVolume = std::max(0.0F, std::min(1.0F, volume));
    SDL_Log("Music volume set to %.2f", mMusicVolume);
}

void AudioManager::PlayTone(float frequency, float duration, float amplitude) {
    if (!mInitialized) {
        return;
    }
    
    ToneData tone;
    tone.frequency = frequency;
    tone.duration = duration;
    tone.amplitude = amplitude;
    tone.phase = 0.0F;
    tone.active = true;
    
    SDL_LockAudioDevice(mAudioDevice);
    mActiveTones.push_back(tone);
    SDL_UnlockAudioDevice(mAudioDevice);
}

void AudioManager::AudioCallback(void* userdata, Uint8* stream, int len) {
    auto* audioManager = static_cast<AudioManager*>(userdata);
    auto* buffer = reinterpret_cast<Sint16*>(stream);
    int samples = len / sizeof(Sint16);
    
    audioManager->GenerateAudio(buffer, samples);
}

void AudioManager::GenerateAudio(Sint16* buffer, int samples) {
    // Clear buffer
    std::fill(buffer, buffer + samples, 0);
    
    // Count active tones for amplitude normalization
    int activeToneCount = 0;
    for (const auto& tone : mActiveTones) {
        if (tone.active) activeToneCount++;
    }
    
    // Prevent division by zero and apply normalization for multiple tones
    float normalizationFactor = (activeToneCount > 0) ? (1.0F / std::sqrt(static_cast<float>(activeToneCount))) : 1.0F;
    
    // Generate sound effects
    for (auto& tone : mActiveTones) {
        if (!tone.active) continue;
        
        for (int i = 0; i < samples; ++i) {
            // Calculate fade envelope to prevent pops
            float fadeTime = 0.01F; // 10ms fade in/out
            float totalDuration = tone.duration + (2.0F * fadeTime);
            float currentTime = totalDuration - tone.duration;
            
            float envelope = 1.0F;
            if (currentTime < fadeTime) {
                // Fade in
                envelope = currentTime / fadeTime;
            } else if (tone.duration < fadeTime) {
                // Fade out
                envelope = tone.duration / fadeTime;
            }
            
            // Generate sine wave with envelope and normalization
            float sampleValue = tone.amplitude * envelope * normalizationFactor * std::sin(tone.phase);
            tone.phase += 2.0F * static_cast<float>(M_PI) * tone.frequency / static_cast<float>(SAMPLE_RATE);
            
            // Keep phase in range to prevent accumulation errors
            if (tone.phase > 2.0F * static_cast<float>(M_PI)) {
                tone.phase -= 2.0F * static_cast<float>(M_PI);
            }
            
            // Convert to 16-bit integer and mix with safe addition
            float currentSample = static_cast<float>(buffer[i]) / 32767.0F;
            float newSample = currentSample + (sampleValue * 0.8F); // Leave headroom
            buffer[i] = static_cast<Sint16>(std::max(-1.0F, std::min(1.0F, newSample)) * 32767.0F);
            
            // Update duration
            tone.duration -= 1.0F / static_cast<float>(SAMPLE_RATE);
            if (tone.duration <= 0.0F) {
                tone.active = false;
                break;
            }
        }
    }
    
    // Generate background music
    if (mMusicPlaying) {
        GenerateBackgroundMusic(buffer, samples);
    }
    
    // Apply master volume and clipping to prevent pops from overflow
    for (int i = 0; i < samples; ++i) {
        // Apply soft clipping to prevent harsh distortion
        float sample = static_cast<float>(buffer[i]) / 32767.0F;
        
        // Soft clipping using tanh for smoother limiting
        if (sample > 0.8F) {
            sample = 0.8F + 0.2F * std::tanh((sample - 0.8F) * 5.0F);
        } else if (sample < -0.8F) {
            sample = -0.8F + 0.2F * std::tanh((sample + 0.8F) * 5.0F);
        }
        
        // Convert back to 16-bit with proper clamping
        int finalSample = static_cast<int>(sample * 32767.0F);
        buffer[i] = static_cast<Sint16>(std::max(-32767, std::min(32767, finalSample)));
    }
}

void AudioManager::GenerateBackgroundMusic(Sint16* buffer, int samples) {
    // Techno-style ambient music - lower pitch, faster tempo, more electronic
    // Using lower octave notes with added harmonics for tech feel
    static const float bassNotes[] = {
        65.41F, 73.42F, 82.41F, 98.00F, 110.00F,   // C2, D2, E2, G2, A2 - bass range
        130.81F, 146.83F, 164.81F, 196.00F, 220.00F // C3, D3, E3, G3, A3 - low mid range
    };
    static const int numNotes = sizeof(bassNotes) / sizeof(bassNotes[0]);
    static float musicPhase1 = 0.0F;
    static float musicPhase2 = 0.0F;
    static float musicPhase3 = 0.0F;
    static int lastNoteIndex = 0;
    static float noteTransition = 0.0F;
    
    for (int i = 0; i < samples; ++i) {
        // Change note every 0.8 seconds (faster tempo)
        int noteIndex = static_cast<int>(mMusicTime * 1.25F) % numNotes;
        
        // Smooth note transitions to prevent pops
        if (noteIndex != lastNoteIndex) {
            noteTransition = 0.0F; // Reset transition
            lastNoteIndex = noteIndex;
        }
        
        // Smooth transition between notes over 50ms
        const float transitionTime = 0.05F;
        if (noteTransition < transitionTime) {
            noteTransition += 1.0F / static_cast<float>(SAMPLE_RATE);
        }
        float transitionSmooth = std::min(1.0F, noteTransition / transitionTime);
        
        float baseFreq = bassNotes[noteIndex];
        
        // Generate techno-style waveform with harmonics using continuous phase
        float wave1 = std::sin(musicPhase1);
        float wave2 = 0.3F * std::sin(musicPhase2);
        float wave3 = 0.15F * std::sin(musicPhase3);
        
        // Update phases continuously to prevent discontinuities
        musicPhase1 += 2.0F * static_cast<float>(M_PI) * baseFreq / static_cast<float>(SAMPLE_RATE);
        musicPhase2 += 2.0F * static_cast<float>(M_PI) * baseFreq * 2.0F / static_cast<float>(SAMPLE_RATE);
        musicPhase3 += 2.0F * static_cast<float>(M_PI) * baseFreq * 3.0F / static_cast<float>(SAMPLE_RATE);
        
        // Keep phases in range
        if (musicPhase1 > 2.0F * static_cast<float>(M_PI)) musicPhase1 -= 2.0F * static_cast<float>(M_PI);
        if (musicPhase2 > 2.0F * static_cast<float>(M_PI)) musicPhase2 -= 2.0F * static_cast<float>(M_PI);
        if (musicPhase3 > 2.0F * static_cast<float>(M_PI)) musicPhase3 -= 2.0F * static_cast<float>(M_PI);
        
        // Create pulsing envelope for electronic feel
        float pulseRate = 4.0F; // 4 pulses per second
        float envelope = 0.3F + 0.7F * (0.5F + 0.5F * std::sin(mMusicTime * 2.0F * static_cast<float>(M_PI) * pulseRate));
        
        // Apply transition smoothing and mix harmonics
        float sampleValue = mMusicVolume * 0.15F * envelope * transitionSmooth * (wave1 + wave2 + wave3);
        
        // Mix with existing audio with proper clamping
        int mixedSample = buffer[i] + static_cast<int>(sampleValue * 32767.0F);
        buffer[i] = static_cast<Sint16>(std::max(-32768, std::min(32767, mixedSample)));
        
        mMusicTime += 1.0F / static_cast<float>(SAMPLE_RATE);
    }
}
