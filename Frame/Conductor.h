#pragma once

class Audio;

// 曲の再生時間を管理する時計（Manager と同じく static クラス）
class Conductor
{
private:
	static Audio* m_Audio;
	static bool      m_Playing;
	static double    m_SongTime;     // なめらかにした曲の時刻（秒）
	static double    m_Bpm;
	static double    m_Offset;       // 遅延補正（秒）

	static long long m_Freq;         // QPC の 1秒あたりのカウント数
	static long long m_LastCounter;  // 前のフレームの QPC

public:
	static void Init();
	static void Start(Audio* audio, double startSec = 0.0);
	static void Pause();
	static void ReStart();
	static void Update();

	static double GetSongTime() { return m_SongTime - m_Offset; }
	static double GetBeat() { return GetSongTime() * m_Bpm / 60.0; }
	static double GetRawAudioTime();   // デバッグ用：補正前の音声時刻

	static void   SetBpm(double bpm) { m_Bpm = bpm; }
	static void   SetOffset(double sec) { m_Offset = sec; }
	static bool   IsPlaying() { return m_Playing; }
};