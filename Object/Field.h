#pragma once
#include "main.h"
#include "GameObject.h"

class Audio;
class FIELD : public GameObject
{
private:
	Audio* m_Bgm{ nullptr };
public:
	// 頂点構造体
	struct Vertex3D {
		XMFLOAT3 Position;
		XMFLOAT3 Normal;
		XMFLOAT4 Diffuse;
		XMFLOAT2 TexCoord;
	};

	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};


