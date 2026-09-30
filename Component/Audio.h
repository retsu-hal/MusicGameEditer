#pragma once

#include <xaudio2.h>
#include "Component.h"


class Audio : public Component
{
private:
	static IXAudio2*				m_Xaudio;
	static IXAudio2MasteringVoice*	m_MasteringVoice;

	IXAudio2SourceVoice*	m_SourceVoice{};
	BYTE*					m_SoundData{};

	int						m_Length{};
	int						m_PlayLength{};

	int 						m_SampleRate{};
	UINT64 				m_BaseSamples{};
	int						m_StartSample{};


public:
	static void InitMaster();
	static void UninitMaster();

	using Component::Component;

	void Uninit() override;

	void Load(const char *FileName);
	void Play(bool Loop = false);
	void PlayFrom(double startSec);
	void Pause();
	void Resume();
	double GetPlaybackTime() const;


};

