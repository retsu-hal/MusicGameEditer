#include "main.h"
#include "Manager.h"
#include "Input.h"
#include "EditorScene.h"
#include "TitleScene.h"
#include "ChartEditor.h"

void EditorScene::Init()
{
	Manager::AddGameObject<ChartEditor>();
}

void EditorScene::Update()
{
	if (Input::GetKeyTrigger(VK_ESCAPE))
		Manager::ChangeScene<TitleScene>();
}
