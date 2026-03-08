#include "AudioManager.h"
#include <iostream>

// ÅyèCê≥ÅzBGMèâä˙âπó Ç0.2Ç…ê›íË
float AudioManager::bgmVolume = 0.005f;
float AudioManager::seVolume = 0.01f;
MusicType AudioManager::currentBGM = BGM_NONE;
std::map<MusicType, Music> AudioManager::musicMap;
std::map<SoundType, Sound> AudioManager::soundMap;

void AudioManager::Init() {
    InitAudioDevice();
    LoadAll();
}

void AudioManager::Close() {
    for (auto& pair : musicMap) UnloadMusicStream(pair.second);
    for (auto& pair : soundMap) UnloadSound(pair.second);
    CloseAudioDevice();
}

void AudioManager::LoadAll() {
    // BGMì«Ç›çûÇ›
    musicMap[BGM_TITLE] = LoadMusicStream("resources/bgm_title.mp3");
    musicMap[BGM_HOME] = LoadMusicStream("resources/bgm_home.mp3");
    musicMap[BGM_DUNGEON] = LoadMusicStream("resources/bgm_dungeon.mp3");

    // SEì«Ç›çûÇ›
    soundMap[SE_ATTACK] = LoadSound("resources/se_attack.wav");
    soundMap[SE_ENEMY_ATTACK] = LoadSound("resources/se_enemy_attack.wav");
    soundMap[SE_CLICK] = LoadSound("resources/se_click.wav");
    soundMap[SE_SKILL] = LoadSound("resources/se_skill.wav");
    soundMap[SE_STAIRS] = LoadSound("resources/se_stairs.wav");
    soundMap[SE_SAVE] = LoadSound("resources/se_save.wav");
    soundMap[SE_REFORGE] = LoadSound("resources/se_reforge.wav");

    // Åyí«â¡ÅzêVãKSE
    soundMap[SE_LEVELUP] = LoadSound("resources/se_levelup.wav");
    soundMap[SE_HEAL] = LoadSound("resources/se_heal.wav");

    // èâä˙É{ÉäÉÖÅ[ÉÄìKóp
    SetBGMVolume(bgmVolume);
    SetSEVolume(seVolume);
}

void AudioManager::Update() {
    if (currentBGM != BGM_NONE && musicMap.count(currentBGM)) {
        UpdateMusicStream(musicMap[currentBGM]);
    }
}

void AudioManager::PlayBGM(MusicType type) {
    if (currentBGM == type) return;

    if (currentBGM != BGM_NONE && musicMap.count(currentBGM)) {
        StopMusicStream(musicMap[currentBGM]);
    }

    currentBGM = type;

    if (currentBGM != BGM_NONE && musicMap.count(currentBGM)) {
        PlayMusicStream(musicMap[currentBGM]);
        SetMusicVolume(musicMap[currentBGM], bgmVolume);
    }
}

void AudioManager::PlaySE(SoundType type) {
    if (soundMap.count(type)) {
        SetSoundVolume(soundMap[type], seVolume);
        PlaySound(soundMap[type]);
    }
}

void AudioManager::SetBGMVolume(float vol) {
    bgmVolume = vol;
    if (bgmVolume < 0.0f) bgmVolume = 0.0f;
    if (bgmVolume > 1.0f) bgmVolume = 1.0f;

    if (currentBGM != BGM_NONE && musicMap.count(currentBGM)) {
        SetMusicVolume(musicMap[currentBGM], bgmVolume);
    }
}

void AudioManager::SetSEVolume(float vol) {
    seVolume = vol;
    if (seVolume < 0.0f) seVolume = 0.0f;
    if (seVolume > 1.0f) seVolume = 1.0f;
}