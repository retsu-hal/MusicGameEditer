#include "Chart.h"
#include <climits>
#include "nlohmann/json.hpp"
#include <fstream>
#include <algorithm>
using json = nlohmann::json;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TempoEvent, tick, bpm)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Note, tick, lane, type, length)

double Chart::TickToSec(int tick) const
{
	double sec = offset;

	// テンポ区間を先頭から順に足していく
	for (size_t i = 0; i < tempos.size(); i++)
	{
		const TempoEvent& cur = tempos[i];

		// この区間の終わり（次の BPM 変化点。なければ無限）
		int endTick = (i + 1 < tempos.size()) ? tempos[i + 1].tick : INT_MAX;

		// 1 tick が何秒か = (60 / bpm) / 480
		double secPerTick = 60.0 / cur.bpm / TICKS_PER_BEAT;

		if (tick < endTick)
		{
			// 目的の tick がこの区間の中にある → 残りを足して終わり
			return sec + (tick - cur.tick) * secPerTick;
		}

		// 区間まるごと足して、次の区間へ
		sec += (endTick - cur.tick) * secPerTick;
	}
	return sec;
}

double Chart::SecToTick(double sec) const
{
	double t = sec - offset;   // 残り時間
	for (size_t i = 0; i < tempos.size(); i++)
	{
		const TempoEvent& cur = tempos[i];
		double secPerTick = 60.0 / cur.bpm / TICKS_PER_BEAT;

		if (i + 1 < tempos.size())
		{
			int    endTick = tempos[i + 1].tick;
			double span = (endTick - cur.tick) * secPerTick;   // この区間の長さ（秒）
			if (t >= span)
			{
				t -= span;
				continue;   // この区間を通り過ぎた
			}
		}
		return cur.tick + t / secPerTick;
	}
	return t / (60.0 / 120.0 / TICKS_PER_BEAT);   // テンポ未設定時の保険
}

bool Chart::Save(const std::string& path) const
{
	json j;                           // 空の JSON（連想配列のように使える）
	j["version"] = 1;              // 形式のバージョン（将来の互換性のため）
	j["title"] = title;
	j["audio"] = audioPath;
	j["offset"] = offset;
	j["resolution"] = TICKS_PER_BEAT;
	j["tempos"] = tempos;         // vector もマクロのおかげでそのまま入る
	j["notes"] = notes;

	std::ofstream ofs(path);
	if (!ofs) return false;           // ファイルを作れなかった
	ofs << j.dump(2);                 // 2 = インデント幅（人が読みやすい形で出力）
	return true;
}

bool Chart::Load(const std::string& path)
{
	std::ifstream ifs(path);
	if (!ifs) return false;           // ファイルが無い

	// 第3引数 false = 書式が壊れていても例外を投げず、is_discarded() で知らせる
	json j = json::parse(ifs, nullptr, false);
	if (j.is_discarded()) return false;

	// value("キー", 無かったときの値)
	title = j.value("title", "");
	audioPath = j.value("audio", "");
	offset = j.value("offset", 0.0);
	tempos = j.value("tempos", std::vector<TempoEvent>{});
	notes = j.value("notes", std::vector<Note>{});

	// 手で編集されて順番がバラバラでも大丈夫なように tick 順に並べ直す
	std::sort(tempos.begin(), tempos.end(),
		[](const TempoEvent& a, const TempoEvent& b) { return a.tick < b.tick; });
	std::sort(notes.begin(), notes.end(),
		[](const Note& a, const Note& b) { return a.tick < b.tick; });

	// TickToSec は「先頭は tick 0」が前提なので、なければ補う
	if (tempos.empty() || tempos[0].tick != 0)
		tempos.insert(tempos.begin(), TempoEvent{ 0, 120.0 });

	return true;
}
