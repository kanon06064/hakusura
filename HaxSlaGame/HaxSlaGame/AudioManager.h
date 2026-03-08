#pragma once
#include "Definitions.h"
#include "raylib.h"
#include <map>
#include <string>

class AudioManager {
public:
    static void Init();
    static void Close();
    static void Update();

    static void LoadAll();

    static void PlayBGM(MusicType type);
    static void PlaySE(SoundType type);

    static void SetBGMVolume(float vol);
    static void SetSEVolume(float vol);

    static float bgmVolume;
    static float seVolume;

private:
    static std::map<MusicType, Music> musicMap;
    static std::map<SoundType, Sound> soundMap;
    static MusicType currentBGM;
};