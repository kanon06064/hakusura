#include "AudioManager.h"
#include <iostream>
#include <string>

float AudioManager::bgmVolume = 0.5f;
float AudioManager::seVolume = 0.5f;
MusicType AudioManager::currentBGM = BGM_NONE;
std::map<MusicType, Music> AudioManager::musicMap;
std::map<SoundType, Sound> AudioManager::soundMap;
std::map<MusicType, float> AudioManager::bgmBaseVolumes;
std::map<SoundType, float> AudioManager::seBaseVolumes;

static std::string FindAudioPath(const std::string& filename) {
    std::string path1 = "resources/Music/" + filename;
    if (FileExists(path1.c_str())) return path1;
    return "resources/" + filename;
}

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
    musicMap[BGM_TITLE] = LoadMusicStream(FindAudioPath("bgm_title.mp3").c_str());
    musicMap[BGM_HOME] = LoadMusicStream(FindAudioPath("bgm_home.mp3").c_str());
    musicMap[BGM_DUNGEON] = LoadMusicStream(FindAudioPath("bgm_dungeon.mp3").c_str());

    bgmBaseVolumes[BGM_TITLE] = 1.0f;
    bgmBaseVolumes[BGM_HOME] = 0.8f;
    bgmBaseVolumes[BGM_DUNGEON] = 0.9f;

    soundMap[SE_ATTACK] = LoadSound(FindAudioPath("se_attack.wav").c_str());
    soundMap[SE_ENEMY_ATTACK] = LoadSound(FindAudioPath("se_enemy_attack.wav").c_str());
    soundMap[SE_CLICK] = LoadSound(FindAudioPath("se_click.wav").c_str());
    soundMap[SE_SKILL] = LoadSound(FindAudioPath("se_skill.wav").c_str());
    soundMap[SE_STAIRS] = LoadSound(FindAudioPath("se_stairs.wav").c_str());
    soundMap[SE_SAVE] = LoadSound(FindAudioPath("se_save.wav").c_str());
    soundMap[SE_REFORGE] = LoadSound(FindAudioPath("se_reforge.wav").c_str());
    soundMap[SE_LEVELUP] = LoadSound(FindAudioPath("se_levelup.wav").c_str());
    soundMap[SE_HEAL] = LoadSound(FindAudioPath("se_heal.wav").c_str());

    seBaseVolumes[SE_ATTACK] = 1.0f;
    seBaseVolumes[SE_ENEMY_ATTACK] = 0.8f;
    seBaseVolumes[SE_CLICK] = 0.6f;
    seBaseVolumes[SE_SKILL] = 1.0f;
    seBaseVolumes[SE_STAIRS] = 0.7f;
    seBaseVolumes[SE_SAVE] = 0.8f;
    seBaseVolumes[SE_REFORGE] = 0.8f;
    seBaseVolumes[SE_LEVELUP] = 0.9f;
    seBaseVolumes[SE_HEAL] = 0.8f;

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
        float base = bgmBaseVolumes.count(currentBGM) ? bgmBaseVolumes[currentBGM] : 1.0f;
        SetMusicVolume(musicMap[currentBGM], bgmVolume * base);
    }
}

void AudioManager::PlaySE(SoundType type) {
    if (soundMap.count(type)) {
        float base = seBaseVolumes.count(type) ? seBaseVolumes[type] : 1.0f;
        SetSoundVolume(soundMap[type], seVolume * base);
        PlaySound(soundMap[type]);
    }
}

void AudioManager::SetBGMVolume(float vol) {
    bgmVolume = vol;
    if (bgmVolume < 0.0f) bgmVolume = 0.0f;
    if (bgmVolume > 1.0f) bgmVolume = 1.0f;

    if (currentBGM != BGM_NONE && musicMap.count(currentBGM)) {
        float base = bgmBaseVolumes.count(currentBGM) ? bgmBaseVolumes[currentBGM] : 1.0f;
        SetMusicVolume(musicMap[currentBGM], bgmVolume * base);
    }
}

void AudioManager::SetSEVolume(float vol) {
    seVolume = vol;
    if (seVolume < 0.0f) seVolume = 0.0f;
    if (seVolume > 1.0f) seVolume = 1.0f;
}