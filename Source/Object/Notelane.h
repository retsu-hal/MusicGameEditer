#pragma once
#include "GameObject.h"
#include "Chart.h"
#include <vector>
#include <string>

class Audio;
struct ImVec2;

// 判定の種類
enum class Judge
{
	None,      // まだ判定していない
	Perfect,
	Great,
	Good,
	Miss,
	Skip,      // 途中から再生したときに飛ばしたノーツ（数えない）
};

// 叩いたときの演出1つ分
struct HitEffect
{
	int   lane;
	Judge judge;
	float time = 0.0f;   // 発生してからの経過時間（秒）
};

class Notelane : public GameObject
{
private:
	Audio* m_Music{ nullptr };
	Chart  m_Chart;
	std::vector<double> m_NoteTimes;   // 各ノーツの時刻（秒）
	std::vector<Judge>  m_Judges;      // 各ノーツの判定結果

	// 見た目の設定
	int   m_LaneCount  = 4;
	float m_LaneWidth  = 120.0f;       // 判定ライン上でのレーン幅（ピクセル）
	float m_JudgeLineY = 600.0f;       // 判定ラインの高さ
	float m_Speed      = 800.0f;       // 1秒で進む奥行き

	// 判定の結果
	int    m_Count[6] = {};
	Judge  m_LastJudge = Judge::None;
	double m_LastDiff  = 0.0;
	double m_DiffSum   = 0.0;
	int    m_HitCount  = 0;

	// 演出
	float m_LaneGlow[4] = {};              // 各レーンの光の強さ（0〜1）
	std::vector<HitEffect> m_Effects;      // 再生中のヒット演出

	double m_LastInputLag = 0.0;           // 入力→判定の遅れ（秒）

	void JudgeLane(int lane, double now);
	void SetJudge(size_t index, Judge judge, double diff);

	// 奥行き z（判定ラインからの距離）のレーン位置 laneX を画面座標に変換
	ImVec2 Project(float laneX, float z) const;

public:
	void Uninit() override;
	void Update() override;
	void Draw() override;

	bool LoadChart(const std::string& path);
	void Play(double startSec = 0.0);
};
