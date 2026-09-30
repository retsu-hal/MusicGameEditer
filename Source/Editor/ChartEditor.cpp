#include "main.h"
#include "ChartEditor.h"
#include "Audio.h"
#include "Conductor.h"
#include <algorithm>
#include <cmath>

// スナップの細かさ：1拍を何分割するか
//   1 = 4分音符, 2 = 8分, 3 = 8分3連(12分), 4 = 16分, 6 = 16分3連(24分), 8 = 32分
static const int   SNAP_DIVS[]   = { 1, 2, 3, 4, 6, 8 };
static const char* SNAP_NAMES[]  = { "1/4", "1/8", "1/12", "1/16", "1/24", "1/32" };
static const int   SNAP_COUNT    = 6;
static const int   TICKS_PER_BAR = TICKS_PER_BEAT * 4;   // 1小節（4/4拍子）

static const float LEFT_MARGIN = 56.0f;   // 左側の小節番号を書く幅


void ChartEditor::Init()
{
	LoadChart();

	// 曲は最初に1回だけ読み込む
	if (!m_Chart.audioPath.empty())
	{
		m_Music = AddComponent<Audio>();
		m_Music->Load(m_Chart.audioPath.c_str());
	}
}


int ChartEditor::GetSnapTicks() const
{
	return TICKS_PER_BEAT / SNAP_DIVS[m_SnapIndex];
}


int ChartEditor::FindNote(int lane, int tick) const
{
	for (size_t i = 0; i < m_Chart.notes.size(); i++)
	{
		const Note& n = m_Chart.notes[i];
		if (n.lane == lane && n.tick == tick) return (int)i;
	}
	return -1;
}


void ChartEditor::LoadChart()
{
	if (m_Chart.Load(m_Path))
	{
		m_Message = "読み込みました";
	}
	else
	{
		// ファイルが無ければ新しい譜面として始める
		m_Chart = Chart();
		m_Chart.tempos = { { 0, 120.0 } };
		m_Message = "ファイルが無いので新規作成します";
	}
	m_Dirty = false;
}


void ChartEditor::SaveChart()
{
	if (m_Chart.Save(m_Path))
	{
		m_Dirty = false;
		m_Message = "保存しました";
	}
	else
	{
		m_Message = "保存に失敗しました";
	}
}


void ChartEditor::StartPlay()
{
	if (!m_Music) return;
	double sec = m_Chart.TickToSec(m_CursorTick);
	if (sec < 0.0) sec = 0.0;
	Conductor::Start(m_Music, sec);
	m_Playing = true;
}


void ChartEditor::StopPlay()
{
	Conductor::Pause();
	m_Playing = false;
}


void ChartEditor::Update()
{
	GameObject::Update();

	ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(560, 680), ImGuiCond_FirstUseEver);

	// 未保存の変更があるとタイトルに * を付ける（### 以降は ImGui 内部の名前なので変わらない）
	std::string title = std::string("譜面エディタ") + (m_Dirty ? " *" : "") + "###ChartEditor";
	ImGui::Begin(title.c_str());

	DrawToolbar();
	ImGui::Separator();
	DrawTimeline();

	ImGui::End();
}


// ------------------------------------------------------------
// 上部の操作パネル
// ------------------------------------------------------------
void ChartEditor::DrawToolbar()
{
	// ファイル
	ImGui::SetNextItemWidth(260);
	ImGui::InputText("##path", m_Path, sizeof(m_Path));
	ImGui::SameLine();
	if (ImGui::Button("読込")) LoadChart();
	ImGui::SameLine();
	if (ImGui::Button("保存") || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)))
		SaveChart();

	// 曲の設定（BPM 変化は 4-3 で対応。今は最初の BPM だけ）
	ImGui::SetNextItemWidth(100);
	if (ImGui::InputDouble("BPM", &m_Chart.tempos[0].bpm, 1.0, 10.0, "%.2f")) m_Dirty = true;
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	if (ImGui::InputDouble("offset(秒)", &m_Chart.offset, 0.001, 0.01, "%.3f")) m_Dirty = true;
	if (m_Chart.tempos[0].bpm < 1.0) m_Chart.tempos[0].bpm = 1.0;   // 0 だと割り算で壊れる

	// 編集の設定
	ImGui::SetNextItemWidth(80);
	ImGui::Combo("スナップ", &m_SnapIndex, SNAP_NAMES, SNAP_COUNT);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120);
	ImGui::SliderFloat("ズーム", &m_PixelsPerBeat, 30.0f, 600.0f, "%.0f");

	// 再生
	if (ImGui::Button(m_Playing ? "■ 停止 (Space)" : "▶ 再生 (Space)"))
		m_Playing ? StopPlay() : StartPlay();
	ImGui::SameLine();
	int bar  = (int)std::floor((double)m_CursorTick / TICKS_PER_BAR) + 1;
	double beat = (double)(m_CursorTick - (bar - 1) * TICKS_PER_BAR) / TICKS_PER_BEAT + 1.0;
	ImGui::Text("カーソル %d小節 %.2f拍 (%.3f秒)  ノーツ %d個",
		bar, beat, m_Chart.TickToSec(m_CursorTick), (int)m_Chart.notes.size());

	if (!m_Message.empty()) ImGui::TextDisabled("%s", m_Message.c_str());
}


// ------------------------------------------------------------
// タイムライン（下から上に時間が進む）
// ------------------------------------------------------------
void ChartEditor::DrawTimeline()
{
	ImGuiIO& io = ImGui::GetIO();

	// 描く場所を確保する（見えないボタンを置いて、クリックやホイールを受け取る）
	ImVec2 p0   = ImGui::GetCursorScreenPos();
	ImVec2 size = ImGui::GetContentRegionAvail();
	if (size.x < 100) size.x = 100;
	if (size.y < 100) size.y = 100;
	ImVec2 p1   = ImVec2(p0.x + size.x, p0.y + size.y);

	ImGui::InvisibleButton("timeline", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	bool hovered = ImGui::IsItemHovered();

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->PushClipRect(p0, p1, true);                         // はみ出た分は描かない
	dl->AddRectFilled(p0, p1, IM_COL32(18, 16, 28, 255));

	const float  pxPerTick = m_PixelsPerBeat / TICKS_PER_BEAT;
	const int    snap      = GetSnapTicks();
	const float  laneLeft  = p0.x + LEFT_MARGIN;
	const float  laneRight = laneLeft + m_LaneWidth * m_LaneCount;

	// tick ⇔ 画面Y の変換（下が過去、上が未来）
	auto TickToY = [&](double tick) { return p1.y - (float)((tick - m_ViewTick) * pxPerTick); };
	auto YToTick = [&](float y)     { return m_ViewTick + (p1.y - y) / pxPerTick; };

	// --- 再生中は再生位置を追いかけてスクロール ---
	double playTick = m_Chart.SecToTick(Conductor::GetSongTime());
	if (m_Playing)
		m_ViewTick = playTick - (size.y * 0.25f) / pxPerTick;   // 再生位置を下から 1/4 の高さに

	// --- レーンの背景 ---
	dl->AddRectFilled(ImVec2(laneLeft, p0.y), ImVec2(laneRight, p1.y), IM_COL32(28, 24, 44, 255));

	// --- グリッド（スナップ単位の線・拍の線・小節の線） ---
	double topTick = YToTick(p0.y);
	int t = (int)std::floor(m_ViewTick / snap) * snap;
	for (; t <= topTick; t += snap)
	{
		float y = TickToY(t);
		if (t % TICKS_PER_BAR == 0)
		{
			dl->AddLine(ImVec2(laneLeft, y), ImVec2(laneRight, y), IM_COL32(220, 210, 255, 200), 2.0f);
			char buf[16];
			snprintf(buf, sizeof(buf), "%d", t / TICKS_PER_BAR + 1);   // 小節番号
			dl->AddText(ImVec2(p0.x + 8, y - 8), IM_COL32(220, 210, 255, 255), buf);
		}
		else if (t % TICKS_PER_BEAT == 0)
			dl->AddLine(ImVec2(laneLeft, y), ImVec2(laneRight, y), IM_COL32(150, 140, 200, 120), 1.0f);
		else
			dl->AddLine(ImVec2(laneLeft, y), ImVec2(laneRight, y), IM_COL32(110, 100, 150, 50), 1.0f);
	}

	// --- レーンの区切り ---
	for (int i = 0; i <= m_LaneCount; i++)
	{
		float x = laneLeft + i * m_LaneWidth;
		dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), IM_COL32(120, 110, 170, 160), 1.0f);
	}

	// --- ノーツ ---
	for (const Note& n : m_Chart.notes)
	{
		float y = TickToY(n.tick);
		if (y < p0.y - 10 || y > p1.y + 10) continue;
		float x = laneLeft + n.lane * m_LaneWidth;
		dl->AddRectFilled(ImVec2(x + 4, y - 5), ImVec2(x + m_LaneWidth - 4, y + 5), IM_COL32(80, 210, 255, 255), 3.0f);
	}

	// --- カーソル（再生開始位置）と再生位置 ---
	float cy = TickToY(m_CursorTick);
	dl->AddLine(ImVec2(p0.x, cy), ImVec2(laneRight + 10, cy), IM_COL32(255, 220, 80, 255), 2.0f);
	if (m_Playing)
	{
		float py = TickToY(playTick);
		dl->AddLine(ImVec2(p0.x, py), ImVec2(laneRight + 10, py), IM_COL32(255, 80, 90, 255), 2.0f);
	}

	// --- マウス操作 ---
	if (hovered)
	{
		ImVec2 m = io.MousePos;
		int tick = (int)std::lround(YToTick(m.y) / snap) * snap;   // 一番近いスナップ位置に合わせる
		int lane = (int)std::floor((m.x - laneLeft) / m_LaneWidth);

		if (lane >= 0 && lane < m_LaneCount)
		{
			// 置く予定の場所を半透明で表示
			float y = TickToY(tick);
			float x = laneLeft + lane * m_LaneWidth;
			dl->AddRectFilled(ImVec2(x + 4, y - 5), ImVec2(x + m_LaneWidth - 4, y + 5), IM_COL32(255, 255, 255, 70), 3.0f);

			// 左クリック：置く ／ 右クリック：消す
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && tick >= 0 && FindNote(lane, tick) < 0)
			{
				Note n;
				n.tick = tick;
				n.lane = lane;
				m_Chart.notes.push_back(n);
				std::sort(m_Chart.notes.begin(), m_Chart.notes.end(),
					[](const Note& a, const Note& b) { return a.tick < b.tick; });
				m_Dirty = true;
			}
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
			{
				int idx = FindNote(lane, tick);
				if (idx >= 0)
				{
					m_Chart.notes.erase(m_Chart.notes.begin() + idx);
					m_Dirty = true;
				}
			}
		}
		else if (m.x < laneLeft && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			// 左の余白をクリック：カーソル（再生開始位置）を移動
			m_CursorTick = std::max(0, tick);
		}

		// ホイール：スクロール ／ Ctrl + ホイール：ズーム
		if (io.MouseWheel != 0.0f)
		{
			if (io.KeyCtrl)
				m_PixelsPerBeat = std::clamp(m_PixelsPerBeat * std::pow(1.15f, io.MouseWheel), 30.0f, 600.0f);
			else if (!m_Playing)
				m_ViewTick += io.MouseWheel * snap * 2;
		}
	}

	// Space：再生／停止（このウィンドウを操作中のときだけ）
	if (ImGui::IsWindowFocused() && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false))
		m_Playing ? StopPlay() : StartPlay();

	dl->PopClipRect();
}
