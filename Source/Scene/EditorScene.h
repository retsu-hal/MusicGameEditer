#pragma once
#include "Scene.h"

// 譜面エディタのシーン（タイトルで F2、Esc でタイトルに戻る）
class EditorScene : public Scene
{
public:
	void Init() override;
	void Update() override;
};
