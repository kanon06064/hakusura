#include "AudioManager.h"
#include <iostream>
#include <string>

// オプションの初期音量を50%に設定
float AudioManager::bgmVolume = 0.5f;
float AudioManager::seVolume = 0.5f;
MusicType AudioManager::currentBGM = BGM_NONE;
std::map<MusicType, Music> AudioManager::musicMap;
std::map<SoundType, Sound> AudioManager::soundMap;

// ベース音量用のマップ実体
std::map<MusicType, float> AudioManager::bgmBaseVolumes;
std::map<SoundType, float> AudioManager::seBaseVolumes;

// サブフォルダと直下の両方を自動検索するヘルパー関数
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
    // BGM読み込み
    musicMap[BGM_TITLE] = LoadMusicStream(FindAudioPath("bgm_title.mp3").c_str());
    musicMap[BGM_HOME] = LoadMusicStream(FindAudioPath("bgm_home.mp3").c_str());
    musicMap[BGM_DUNGEON] = LoadMusicStream(FindAudioPath("bgm_dungeon.mp3").c_str());

    // ★BGMごとのベース音量を設定 (素材ごとに異なる音圧をここで均一化・ラッピング)
    bgmBaseVolumes[BGM_TITLE] = 1.0f;
    bgmBaseVolumes[BGM_HOME] = 0.8f;
    bgmBaseVolumes[BGM_DUNGEON] = 0.9f;

    // SE読み込み
    soundMap[SE_ATTACK] = LoadSound(FindAudioPath("se_attack.wav").c_str());
    soundMap[SE_ENEMY_ATTACK] = LoadSound(FindAudioPath("se_enemy_attack.wav").c_str());
    soundMap[SE_CLICK] = LoadSound(FindAudioPath("se_click.wav").c_str());
    soundMap[SE_SKILL] = LoadSound(FindAudioPath("se_skill.wav").c_str());
    soundMap[SE_STAIRS] = LoadSound(FindAudioPath("se_stairs.wav").c_str());
    soundMap[SE_SAVE] = LoadSound(FindAudioPath("se_save.wav").c_str());
    soundMap[SE_REFORGE] = LoadSound(FindAudioPath("se_reforge.wav").c_str());
    soundMap[SE_LEVELUP] = LoadSound(FindAudioPath("se_levelup.wav").c_str());
    soundMap[SE_HEAL] = LoadSound(FindAudioPath("se_heal.wav").c_str());

    // ★SEごとのベース音量を設定 (うるさい音は下げて、小さい音は上げる)
    seBaseVolumes[SE_ATTACK] = 1.0f;
    seBaseVolumes[SE_ENEMY_ATTACK] = 0.8f;
    seBaseVolumes[SE_CLICK] = 0.6f;  // UIクリック音はうるさくなりやすいので控えめに
    seBaseVolumes[SE_SKILL] = 1.0f;
    seBaseVolumes[SE_STAIRS] = 0.7f;
    seBaseVolumes[SE_SAVE] = 0.8f;
    seBaseVolumes[SE_REFORGE] = 0.8f;
    seBaseVolumes[SE_LEVELUP] = 0.9f;
    seBaseVolumes[SE_HEAL] = 0.8f;

    // ボリューム初期設定
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

        // ★ユーザー設定音量 × BGM固有のベース音量 で最終的な音量を決定し再生
        float base = bgmBaseVolumes.count(currentBGM) ? bgmBaseVolumes[currentBGM] : 1.0f;
        SetMusicVolume(musicMap[currentBGM], bgmVolume * base);
    }
}

void AudioManager::PlaySE(SoundType type) {
    if (soundMap.count(type)) {
        // ★ユーザー設定音量 × SE固有のベース音量 で最終的な音量を決定し再生
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
        // ★再生中のBGMの音量を変更する際もベース音量を反映させる
        float base = bgmBaseVolumes.count(currentBGM) ? bgmBaseVolumes[currentBGM] : 1.0f;
        SetMusicVolume(musicMap[currentBGM], bgmVolume * base);
    }
}

void AudioManager::SetSEVolume(float vol) {
    seVolume = vol;
    if (seVolume < 0.0f) seVolume = 0.0f;
    if (seVolume > 1.0f) seVolume = 1.0f;
}