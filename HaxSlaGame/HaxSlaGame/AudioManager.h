#pragma once
#include "Definitions.h"
#include "raylib.h"
#include <map>
#include <string>

class AudioManager {
public:
    static void Init();    // オーディオデバイスの初期化
    static void Close();   // オーディオデバイスの終了とメモリ解放
    static void Update();  // 毎フレーム呼ぶ(BGMのストリーム再生用)

    static void LoadAll(); // 全ての音声ファイルをメモリに読み込む

    static void PlayBGM(MusicType type);
    static void PlaySE(SoundType type);

    static void SetBGMVolume(float vol);
    static void SetSEVolume(float vol);

    static float bgmVolume; // ユーザーが設定したBGM音量 (0.0 ~ 1.0)
    static float seVolume;  // ユーザーが設定したSE音量 (0.0 ~ 1.0)

private:
    static std::map<MusicType, Music> musicMap;
    static std::map<SoundType, Sound> soundMap;
    static MusicType currentBGM;

    // 素材ごとに異なる音圧を均一化するための基準(ベース)音量
    static std::map<MusicType, float> bgmBaseVolumes;
    static std::map<SoundType, float> seBaseVolumes;
};