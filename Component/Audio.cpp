
#include "main.h"
#include "Audio.h"





IXAudio2*				Audio::m_Xaudio = NULL;
IXAudio2MasteringVoice*	Audio::m_MasteringVoice = NULL;


void Audio::InitMaster()
{
	// COM初期化
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	// XAudio生成
	XAudio2Create(&m_Xaudio, 0);

	// マスタリングボイス生成
	m_Xaudio->CreateMasteringVoice(&m_MasteringVoice);
}


void Audio::UninitMaster()
{
	m_MasteringVoice->DestroyVoice();
	m_Xaudio->Release();
	CoUninitialize();
}









void Audio::Load(const char *FileName)
{
	// サウンドデータ読込
	WAVEFORMATEX wfx = { 0 };


	{
		HMMIO hmmio = NULL;
		MMIOINFO mmioinfo = { 0 };
		MMCKINFO riffchunkinfo = { 0 };
		MMCKINFO datachunkinfo = { 0 };
		MMCKINFO mmckinfo = { 0 };
		UINT32 buflen;
		LONG readlen;


		hmmio = mmioOpen((LPSTR)FileName, &mmioinfo, MMIO_READ);
		assert(hmmio);

		riffchunkinfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');
		mmioDescend(hmmio, &riffchunkinfo, NULL, MMIO_FINDRIFF);

		mmckinfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
		mmioDescend(hmmio, &mmckinfo, &riffchunkinfo, MMIO_FINDCHUNK);

		if (mmckinfo.cksize >= sizeof(WAVEFORMATEX))
		{
			mmioRead(hmmio, (HPSTR)&wfx, sizeof(wfx));
		}
		else
		{
			PCMWAVEFORMAT pcmwf = { 0 };
			mmioRead(hmmio, (HPSTR)&pcmwf, sizeof(pcmwf));
			memset(&wfx, 0x00, sizeof(wfx));
			memcpy(&wfx, &pcmwf, sizeof(pcmwf));
			wfx.cbSize = 0;
		}
		mmioAscend(hmmio, &mmckinfo, 0);

		datachunkinfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
		mmioDescend(hmmio, &datachunkinfo, &riffchunkinfo, MMIO_FINDCHUNK);



		buflen = datachunkinfo.cksize;
		m_SoundData = new unsigned char[buflen];
		readlen = mmioRead(hmmio, (HPSTR)m_SoundData, buflen);


		m_Length = readlen;
		m_PlayLength = readlen / wfx.nBlockAlign;


		mmioClose(hmmio, 0);
		m_SampleRate = wfx.nSamplesPerSec;
	}


	// サウンドソース生成
	m_Xaudio->CreateSourceVoice(&m_SourceVoice, &wfx);
	assert(m_SourceVoice);
}


void Audio::Uninit()
{
	m_SourceVoice->Stop();
	m_SourceVoice->DestroyVoice();

	delete[] m_SoundData;
}





void Audio::Play(bool Loop)
{
	m_SourceVoice->Stop();
	m_SourceVoice->FlushSourceBuffers();


	// バッファ設定
	XAUDIO2_BUFFER bufinfo;

	memset(&bufinfo, 0x00, sizeof(bufinfo));
	bufinfo.AudioBytes = m_Length;
	bufinfo.pAudioData = m_SoundData;
	bufinfo.PlayBegin = 0;
	bufinfo.PlayLength = m_PlayLength;

	// ループ設定
	if (Loop)
	{
		bufinfo.LoopBegin = 0;
		bufinfo.LoopLength = m_PlayLength;
		bufinfo.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	m_SourceVoice->SubmitSourceBuffer(&bufinfo, NULL);

/*
	float outputMatrix[4] = { 0.0f , 0.0f, 1.0f , 0.0f };
	m_SourceVoice->SetOutputMatrix(m_MasteringVoice, 2, 2, outputMatrix);
	//m_SourceVoice->SetVolume(0.1f);
*/


	// 再生
	m_SourceVoice->Start();

}

void Audio::PlayFrom(double startSec)
{
	// いったん止めて、キューに残っている音を捨てる
	m_SourceVoice->Stop();
	m_SourceVoice->FlushSourceBuffers();

	// 秒 → サンプル位置 に変換
	m_StartSample = (int)(startSec * m_SampleRate);
	if (m_StartSample < 0) m_StartSample = 0;
	if (m_StartSample >= m_PlayLength) return;   // 曲の長さを超えていたら何もしない

	XAUDIO2_BUFFER bufinfo{};                    // {} で全部0にする（memsetと同じ）
	bufinfo.AudioBytes = m_Length;
	bufinfo.pAudioData = m_SoundData;
	bufinfo.PlayBegin = m_StartSample;                  // ここから鳴らす
	bufinfo.PlayLength = m_PlayLength - m_StartSample;   // 残り全部

	// ★ここが重要：再生開始時点のカウンタを覚えておく
	XAUDIO2_VOICE_STATE state;
	m_SourceVoice->GetState(&state);
	m_BaseSamples = state.SamplesPlayed;

	m_SourceVoice->SubmitSourceBuffer(&bufinfo);
	m_SourceVoice->Start();
}

void Audio::Pause()
{
	m_SourceVoice->Stop();    // 一時停止（Flush しないので位置は保たれる）
}

void Audio::Resume()
{
	m_SourceVoice->Start();   // 続きから再生
}

double Audio::GetPlaybackTime() const
{
	if (m_SampleRate == 0) return 0.0;   // Load前なら0

	XAUDIO2_VOICE_STATE state;
	m_SourceVoice->GetState(&state);

	// 今回の再生で進んだサンプル数
	UINT64 played = state.SamplesPlayed - m_BaseSamples;

	// 開始位置 + 進んだ分 を秒に変換
	return (m_StartSample + played) / (double)m_SampleRate;
}