#include "main.h"
#include "Renderer.h"
#include "Field.h"
#include "Component.h"
#include "Audio.h"
#include "Conductor.h"
#include "Chart.h"



void FIELD::Init()
{
	m_Layer = 1;
	m_Position = { 0.0f, 0.0f, 0.0f };
	m_Scale = {20.0f, 1.0f, 20.0f };
	static const XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

	static const FIELD::Vertex3D vertexData[] =
	{
		// Position                      Normal                TexCoord
		// 上面 (Y+)
		{ {-1.0f,  0.0f,  1.0f }, { 0.0f,  1.0f,  0.0f }, color, {  0.0f,  0.0f } },
		{ { 1.0f,  0.0f,  1.0f }, { 0.0f,  1.0f,  0.0f }, color, { 1.0f,  0.0f } },
		{ {-1.0f,  0.0f, -1.0f }, { 0.0f,  1.0f,  0.0f }, color, {  0.0f, 1.0f } },
		{ { 1.0f,  0.0f, -1.0f }, { 0.0f,  1.0f,  0.0f }, color, { 1.0f, 1.0f } },
	};
	Vertex3D vertex[4];

	// vertex配列へコピー
	memcpy(vertex, vertexData, sizeof(vertexData));

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VERTEX_3D) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_vertexBuffer);
	//シェーダー読み込み
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
	Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");

	//テクスチャ読み込み
	TexMetadata metadata;
	ScratchImage image;
	LoadFromWICFile(L"asset\\texture\\iaigami.jpg", WIC_FLAGS_NONE, &metadata, image);//テクスチャは変更可
	CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
		image.GetImageCount(), metadata, &m_Texture);
	assert(m_Texture);//読み込み失敗時にダイアログを表示

	m_Bgm = AddComponent<Audio>();
	m_Bgm->Load("asset\\audio\\click120.wav");

	Conductor::SetBpm(120.0);
	Conductor::Start(m_Bgm, 0.0);

	m_Chart.tempos= { {0, 120.0} ,{480, 120.0} };

	// 1. 譜面を作って保存（1拍ごとに 0→1→2→3 レーンの順でノーツ、8小節）
	Chart save;
	save.title = "テスト譜面";
	save.audioPath = "asset\\audio\\click120.wav";
	save.tempos = { {0, 120.0} };
	for (int i = 0; i < 32; i++)
		save.notes.push_back({ i * 480, i % 4 });
	save.Save("asset\\chart\\test.json");

	// 2. 別の Chart に読み込む
	m_Chart.Load("asset\\chart\\test.json");
}
void FIELD::Uninit()
{
	m_vertexBuffer->Release();

	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_Texture->Release();

	GameObject::Uninit();
}

void FIELD::Update()
{
	GameObject::Update();

	ImGui::Begin("AudioDebug");
	ImGui::Text("audio : %.3f", Conductor::GetRawAudioTime());  // 補正前（カクカク）
	ImGui::Text("song  : %.3f", Conductor::GetSongTime());      // 補正後（なめらか）

	double beat = Conductor::GetBeat();
	double frac = beat - floor(beat);             // 拍の中のどのあたりか（0.0〜1.0）
	bool   isBar = ((int)floor(beat) % 4) == 0;   // 小節の頭か

	ImVec4 color = ImVec4(0.2f, 0.2f, 0.2f, 1);   // 普段は灰色
	if (frac < 0.1)                                // 拍の直後だけ光らせる
		color = isBar ? ImVec4(1, 0.3f, 0.3f, 1) : ImVec4(1, 1, 1, 1);
	ImGui::ColorButton("##beat", color, 0, ImVec2(80, 80));

	static float offsetMs = 0.0f;
	if (ImGui::SliderFloat("offset(ms)", &offsetMs, -200.0f, 200.0f))
		Conductor::SetOffset(offsetMs / 1000.0);

	if (ImGui::Button("Pause"))  Conductor::Pause();
	if (ImGui::Button("ReStart")) Conductor::ReStart();
	if (ImGui::Button("10sec"))  Conductor::Start(m_Bgm, 10.0);
	ImGui::Separator();
	ImGui::Text("tick 960  -> %.3f sec (2拍目 = 1.000)", m_Chart.TickToSec(960));
	ImGui::Text("tick 1920 -> %.3f sec (4拍   = 2.000)", m_Chart.TickToSec(1920));
	ImGui::Text("tick 3840 -> %.3f sec (+4拍@240 = 3.000)", m_Chart.TickToSec(3840));
	ImGui::Text("2.5 sec   -> %.1f tick (= 2880)", m_Chart.SecToTick(2.5));
	ImGui::Text("title : %s", m_Chart.title.c_str());
	ImGui::Text("notes : %d", (int)m_Chart.notes.size());   // 32 なら OK
	ImGui::End();
}

void FIELD::Draw()
{
	// カリング無効化（両面描画）
	D3D11_RASTERIZER_DESC rasterDesc{};
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;  // ← 裏面もカリングしない
	rasterDesc.FrontCounterClockwise = FALSE;
	rasterDesc.DepthClipEnable = TRUE;

	ID3D11RasterizerState* pRasterState = nullptr;
	Renderer::GetDevice()->CreateRasterizerState(&rasterDesc, &pRasterState);
	Renderer::GetDeviceContext()->RSSetState(pRasterState);


	//入力レイアウト設定
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設定
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

	//マトリックス設定
	XMMATRIX WorldMatrix, ScaleMatrix, RotMatrix, TransMatrix;
	ScaleMatrix = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	RotMatrix = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	TransMatrix = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	WorldMatrix = ScaleMatrix * RotMatrix * TransMatrix;

	Renderer::SetWorldMatrix(WorldMatrix);

	//マテリアル設定
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = true;			//true:テクスチャを使用する、false:テクスチャを使用しない
	Renderer::SetMaterial(material);


	//テクスチャ設定
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);
	//頂点バッファ設定
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);

	//プリミティブトポロジ設定
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//描画
	Renderer::GetDeviceContext()->Draw(4, 0);
}