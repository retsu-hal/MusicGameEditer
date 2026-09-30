#pragma once
#include <vector>
#include <string>

constexpr int TICKS_PER_BEAT = 480;   // 1拍 = 480 tick

// BPM の変化点
struct TempoEvent
{
	int    tick = 0;   // どの位置から
	double bpm = 120.0; // この BPM になる
};

// ノーツ1つ
struct Note
{
	int tick = 0;        // 位置
	int lane = 0;        // レーン番号（0〜）
	int type = 0;    // 0:タップ 1:ロング など（あとで増やす）
	int length = 0;  // ロングノーツの長さ（tick）
};

class Chart
{
public:
	std::string title;
	std::string audioPath;
	double offset = 0.0;              // 曲の頭から1拍目までの秒数（曲ごとに違う）

	std::vector<TempoEvent> tempos;   // tick 順に並べる。先頭は必ず tick 0
	std::vector<Note>       notes;

	double TickToSec(int tick) const;  // tick → 秒
	double SecToTick(double sec) const;// 秒 → tick（小数あり。ノーツ移動用）

	bool Save(const std::string& path) const;
	bool Load(const std::string& path);
};