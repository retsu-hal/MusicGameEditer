#include "main.h"
#include "Notelane.h"
#include "Audio.h"
#include "Conductor.h"
#include "Input.h"
#include "Manager.h"
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------
// 判定の設定
// ---------------------------------------------------------------
static constexpr double WINDOW_PERFECT = 0.033;   // ±33ms
static constexpr double WINDOW_GREAT   = 0.066;   // ±66ms
static constexpr double WINDOW_GOOD    = 0.100;   // ±100ms

static const BYTE  LANE_KEYS[4]  = { 'D', 'F', 'J', 'K' };
static const char* LANE_NAMES[4] = { "D", "F", "J", "K" };

// ---------------------------------------------------------------
// 見た目の設定（奥行き）
// ---------------------------------------------------------------
static constexpr float CAMERA_DIST = 600.0f;    // 大きいほど奥行きがゆるやかになる
static constexpr float HORIZON_Y   = 90.0f;     // レーンが収束していく先（地平線）の高さ
static constexpr float Z_FAR       = 2400.0f;   // レーンを描く一番奥
static constexpr float Z_NEAR      = -110.0f;   // 判定ラインより手前（キー表示の部分）
static constexpr float EFFECT_TIME = 0.35f;     // ヒット演出の長さ（秒）

// ---------------------------------------------------------------
// 描画の補助関数（このファイルの中だけで使う）
// ---------------------------------------------------------------
namespace
{
	// r,g,b と 不透明度 a(0〜1) から色を作る
	ImU32 Col(int r, int g, int b, float a)
	{
		a = std::clamp(a, 0.0f, 1.0f);
		return IM_COL32(r, g, b, (int)(a * 255));
	}

	// 4つの頂点それぞれに色を付けた四角形（台形も可）を描く
	// 頂点の順番：p0 左上 → p1 右上 → p2 右下 → p3 左下
	void QuadGradient(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImVec2 p2, ImVec2 p3,
		ImU32 c0, ImU32 c1, ImU32 c2, ImU32 c3)
	{
		ImVec2 uv = ImGui::GetFontTexUvWhitePixel();   // 真っ白な点（テクスチャを使わず色だけで塗る）
		dl->PrimReserve(6, 4);                         // 三角形2つ = 頂点番号6個、頂点4個
		ImDrawIdx i = (ImDrawIdx)dl->_VtxCurrentIdx;
		dl->PrimWriteIdx(i);     dl->PrimWriteIdx(i + 1); dl->PrimWriteIdx(i + 2);
		dl->PrimWriteIdx(i);     dl->PrimWriteIdx(i + 2); dl->PrimWriteIdx(i + 3);
		dl->PrimWriteVtx(p0, uv, c0);
		dl->PrimWriteVtx(p1, uv, c1);
		dl->PrimWriteVtx(p2, uv, c2);
		dl->PrimWriteVtx(p3, uv, c3);
	}

	// 光る線：太くて薄い線を何本か重ねてから、細くて明るい芯を描く
	void GlowLine(ImDrawList* dl, ImVec2 a, ImVec2 b, int r, int g, int bl, float alpha, float thick)
	{
		dl->AddLine(a, b, Col(r, g, bl, 0.08f * alpha), thick * 8.0f);
		dl->AddLine(a, b, Col(r, g, bl, 0.15f * alpha), thick * 4.0f);
		dl->AddLine(a, b, Col(r, g, bl, 0.50f * alpha), thick * 2.0f);
		dl->AddLine(a, b, Col(255, 255, 255, 0.90f * alpha), thick);
	}
}


// 奥行き z のときの画面座標を求める（遠近法）
//   s = 見た目の縮み具合。z=0（判定ライン）で 1、遠いほど 0 に近づく
ImVec2 Notelane::Project(float laneX, float z) const
{
	float s  = CAMERA_DIST / (CAMERA_DIST + z);
	float cx = SCREEN_WIDTH * 0.5f;
	float x  = cx + (laneX - m_LaneCount * 0.5f) * m_LaneWidth * s;
	float y  = HORIZON_Y + (m_JudgeLineY - HORIZON_Y) * s;
	return ImVec2(x, y);
}


bool Notelane::LoadChart(const std::string& path)
{
	if (!m_Chart.Load(path)) return false;

	m_NoteTimes.clear();
	for (const Note& n : m_Chart.notes)
		m_NoteTimes.push_back(m_Chart.TickToSec(n.tick));

	m_Music = AddComponent<Audio>();
	m_Music->Load(m_Chart.audioPath.c_str());
	return true;
}


void Notelane::Play(double startSec)
{
	if (!m_Music) return;

	m_Judges.assign(m_Chart.notes.size(), Judge::None);
	for (int& c : m_Count) c = 0;
	m_LastJudge = Judge::None;
	m_DiffSum = 0.0;
	m_HitCount = 0;
	m_Effects.clear();

	for (size_t i = 0; i < m_NoteTimes.size(); i++)
		if (m_NoteTimes[i] < startSec) m_Judges[i] = Judge::Skip;

	Conductor::Start(m_Music, startSec);
}


void Notelane::Uninit()
{
	Conductor::Stop();
	GameObject::Uninit();
}


void Notelane::JudgeLane(int lane, double now)
{
	for (size_t i = 0; i < m_Chart.notes.size(); i++)
	{
		if (m_Judges[i] != Judge::None) continue;
		if (m_Chart.notes[i].lane != lane) continue;

		double diff = now - m_NoteTimes[i];
		if (diff < -WINDOW_GOOD) return;

		double a = std::fabs(diff);
		if      (a <= WINDOW_PERFECT) SetJudge(i, Judge::Perfect, diff);
		else if (a <= WINDOW_GREAT)   SetJudge(i, Judge::Great,   diff);
		else                          SetJudge(i, Judge::Good,    diff);
		return;
	}
}


void Notelane::SetJudge(size_t index, Judge judge, double diff)
{
	m_Judges[index] = judge;
	m_Count[(int)judge]++;
	m_LastJudge = judge;

	if (judge != Judge::Miss)
	{
		m_LastDiff = diff;
		m_DiffSum += diff;
		m_HitCount++;
	}

	// 演出を追加（Miss でも文字だけ出す）
	HitEffect fx;
	fx.lane  = m_Chart.notes[index].lane;
	fx.judge = judge;
	m_Effects.push_back(fx);
}


void Notelane::Update()
{
	GameObject::Update();

	double now = Conductor::GetSongTime();
	float  dt  = Manager::GetDeltaTime();

	// --- 判定 ---
	if (!m_Judges.empty())
	{
		// 1. キーが押されたレーンを判定（押された瞬間の時刻を使う）
		for (const KeyEvent& e : Input::GetKeyEvents())
		{
			for (int lane = 0; lane < m_LaneCount; lane++)
			{
				if (e.key == LANE_KEYS[lane])
				{
					double hitTime = Conductor::CounterToSongTime(e.time);
					m_LastInputLag = now - hitTime;        // 何秒前に押されていたか（確認用）
					JudgeLane(lane, hitTime);
				}
			}
		}
	}

	// --- レーンの光：押している間は最大、離したら少しずつ消える ---
	for (int lane = 0; lane < m_LaneCount; lane++)
	{
		if (Input::GetKeyPress(LANE_KEYS[lane])) m_LaneGlow[lane] = 1.0f;
		else m_LaneGlow[lane] = std::max(0.0f, m_LaneGlow[lane] - dt * 6.0f);
	}

	// --- ヒット演出：時間を進めて、終わったものを消す ---
	for (HitEffect& fx : m_Effects) fx.time += dt;
	m_Effects.erase(
		std::remove_if(m_Effects.begin(), m_Effects.end(),
			[](const HitEffect& fx) { return fx.time > EFFECT_TIME; }),
		m_Effects.end());

	// --- デバッグ表示 ---
	ImGui::Begin("NoteField");
	ImGui::Text("song  : %.3f", Conductor::GetSongTime());
	ImGui::SliderFloat("speed", &m_Speed, 200.0f, 2000.0f);

	static float offsetMs = 0.0f;
	if (ImGui::SliderFloat("offset(ms)", &offsetMs, -200.0f, 200.0f))
		Conductor::SetOffset(offsetMs / 1000.0);

	if (ImGui::Button("最初から")) Play(0.0);
	ImGui::SameLine();
	if (ImGui::Button("10秒から")) Play(10.0);

	ImGui::Separator();
	ImGui::Text("Perfect : %d", m_Count[(int)Judge::Perfect]);
	ImGui::Text("Great   : %d", m_Count[(int)Judge::Great]);
	ImGui::Text("Good    : %d", m_Count[(int)Judge::Good]);
	ImGui::Text("Miss    : %d", m_Count[(int)Judge::Miss]);
	if (m_HitCount > 0)	ImGui::Text("平均ずれ : %+.1f ms（＋は遅い）", m_DiffSum / m_HitCount * 1000.0);
	ImGui::Text("入力→判定の遅れ : %.1f ms", m_LastInputLag * 1000.0);
	ImGui::End();
}


void Notelane::Draw()
{
	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	const int   N   = m_LaneCount;
	const float now = (float)Conductor::GetSongTime();

	// ============ 1. 床 ============
	// 手前は濃く、奥に行くほど透明にして闇に溶かす
	QuadGradient(dl,
		Project(0, Z_FAR), Project((float)N, Z_FAR), Project((float)N, Z_NEAR), Project(0, Z_NEAR),
		Col(20, 10, 40, 0.0f), Col(20, 10, 40, 0.0f), Col(20, 15, 45, 0.95f), Col(20, 15, 45, 0.95f));

	// ============ 2. 押しているレーンの光 ============
	for (int i = 0; i < N; i++)
	{
		if (m_LaneGlow[i] <= 0.0f) continue;
		float g = m_LaneGlow[i];
		QuadGradient(dl,
			Project((float)i, Z_FAR * 0.6f), Project((float)i + 1, Z_FAR * 0.6f),
			Project((float)i + 1, 0), Project((float)i, 0),
			Col(120, 90, 255, 0.0f), Col(120, 90, 255, 0.0f),
			Col(150, 120, 255, 0.45f * g), Col(150, 120, 255, 0.45f * g));
	}

	// ============ 3. 拍の線（手前に流れてくる横線） ============
	{
		int beat = (int)std::floor(m_Chart.SecToTick(now) / TICKS_PER_BEAT);
		for (;; beat++)
		{
			float z = ((float)m_Chart.TickToSec(beat * TICKS_PER_BEAT) - now) * m_Speed;
			if (z > Z_FAR) break;
			if (z < 0.0f) continue;
			bool  bar  = (beat % 4 == 0);                 // 小節の頭は明るく
			float fade = 1.0f - z / Z_FAR;                // 奥ほど薄く
			dl->AddLine(Project(0, z), Project((float)N, z),
				Col(170, 150, 255, (bar ? 0.35f : 0.12f) * fade), bar ? 2.0f : 1.0f);
		}
	}

	// ============ 4. レーンの区切り線と 5. 両端のネオン ============
	// 奥に行くほど薄くするため、線を短く区切って少しずつ透明にしながら描く
	const int SEG = 24;
	for (int k = 0; k < SEG; k++)
	{
		float z0 = Z_NEAR + (Z_FAR - Z_NEAR) * k / SEG;
		float z1 = Z_NEAR + (Z_FAR - Z_NEAR) * (k + 1) / SEG;
		float fade = 1.0f - std::max(0.0f, z0) / Z_FAR;   // 手前 1 → 奥 0
		fade = fade * fade;                                // 奥の方を早めに暗くする

		for (int i = 1; i < N; i++)
			dl->AddLine(Project((float)i, z0), Project((float)i, z1), Col(160, 140, 230, 0.25f * fade), 1.0f);

		GlowLine(dl, Project(0, z0), Project(0, z1), 190, 90, 255, fade, 2.0f);
		GlowLine(dl, Project((float)N, z0), Project((float)N, z1), 255, 70, 190, fade, 2.0f);
	}

	// ============ 6. ノーツ ============
	for (size_t i = 0; i < m_Chart.notes.size(); i++)
	{
		if (!m_Judges.empty() && m_Judges[i] != Judge::None) continue;

		float z = ((float)m_NoteTimes[i] - now) * m_Speed;
		if (z > Z_FAR || z < -60.0f) continue;

		float lane = (float)m_Chart.notes[i].lane;
		float fade = std::clamp((Z_FAR - z) / (Z_FAR * 0.3f), 0.0f, 1.0f);   // 奥から浮かび上がる
		float h    = 14.0f;                                                  // ノーツの厚み（奥行き方向）

		ImVec2 p0 = Project(lane + 0.06f, z + h), p1 = Project(lane + 0.94f, z + h);
		ImVec2 p2 = Project(lane + 0.94f, z - h), p3 = Project(lane + 0.06f, z - h);

		// 外側の光
		QuadGradient(dl, Project(lane, z + h * 3), Project(lane + 1, z + h * 3),
			Project(lane + 1, z - h * 3), Project(lane, z - h * 3),
			Col(80, 220, 255, 0.0f), Col(80, 220, 255, 0.0f),
			Col(80, 220, 255, 0.25f * fade), Col(80, 220, 255, 0.25f * fade));
		// 本体
		QuadGradient(dl, p0, p1, p2, p3,
			Col(120, 230, 255, fade), Col(120, 230, 255, fade),
			Col(40, 140, 255, fade), Col(40, 140, 255, fade));
		// 白い芯
		dl->AddLine(Project(lane + 0.1f, z), Project(lane + 0.9f, z), Col(255, 255, 255, 0.9f * fade), 2.0f);
	}

	// ============ 7. 判定ライン ============
	GlowLine(dl, Project(0, 0), Project((float)N, 0), 255, 120, 230, 1.0f, 3.0f);

	// ============ 8. キーの枠 ============
	for (int i = 0; i < N; i++)
	{
		ImVec2 a = Project(i + 0.12f, -20.0f);    // 左上
		ImVec2 b = Project(i + 0.88f, -80.0f);    // 右下
		float  g = m_LaneGlow[i];
		dl->AddRectFilled(a, b, Col(150, 120, 255, 0.10f + 0.35f * g), 6.0f);
		dl->AddRect(a, b, Col(200, 190, 255, 0.6f + 0.4f * g), 6.0f, 0, 2.0f);

		float  size = 26.0f;
		ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, LANE_NAMES[i]);
		dl->AddText(ImGui::GetFont(), size,
			ImVec2((a.x + b.x - ts.x) * 0.5f, (a.y + b.y - ts.y) * 0.5f),
			Col(255, 255, 255, 0.8f + 0.2f * g), LANE_NAMES[i]);
	}

	// ============ 9. ヒット演出 ============
	for (const HitEffect& fx : m_Effects)
	{
		float t = fx.time / EFFECT_TIME;     // 0 → 1 に進む
		float a = 1.0f - t;                  // だんだん消える
		float L = (float)fx.lane;

		if (fx.judge != Judge::Miss)
		{
			// 光の柱：判定ラインから上に伸びて、細くなりながら消える
			float w = 0.5f * (1.0f - 0.6f * t);
			QuadGradient(dl,
				Project(L + 0.5f - w, Z_FAR * 0.5f), Project(L + 0.5f + w, Z_FAR * 0.5f),
				Project(L + 0.5f + w, 0), Project(L + 0.5f - w, 0),
				Col(120, 240, 255, 0.0f), Col(120, 240, 255, 0.0f),
				Col(200, 250, 255, 0.8f * a), Col(200, 250, 255, 0.8f * a));

			// 判定ライン上の閃光：広がりながら消える
			ImVec2 c = Project(L + 0.5f, 0);
			dl->AddCircleFilled(c, 20.0f + 50.0f * t, Col(160, 240, 255, 0.35f * a));
			dl->AddCircleFilled(c, 8.0f + 12.0f * t, Col(255, 255, 255, 0.9f * a));
		}

		// 判定の文字：そのレーンの少し上に出て、ふわっと上がる
		const char* text = "";
		ImU32 color = 0;
		switch (fx.judge)
		{
		case Judge::Perfect: text = "PERFECT"; color = Col(255, 230, 120, a); break;
		case Judge::Great:   text = "GREAT";   color = Col(120, 255, 170, a); break;
		case Judge::Good:    text = "GOOD";    color = Col(120, 190, 255, a); break;
		case Judge::Miss:    text = "MISS";    color = Col(180, 180, 190, a); break;
		default: break;
		}
		float  size = 22.0f;
		ImVec2 pos  = Project(L + 0.5f, 90.0f + 60.0f * t);
		ImVec2 ts   = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
		dl->AddText(ImGui::GetFont(), size, ImVec2(pos.x - ts.x * 0.5f, pos.y - ts.y * 0.5f), color, text);
	}
}
