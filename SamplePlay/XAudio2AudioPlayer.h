#pragma once

#include "Interface.h"

#include <xaudio2.h>
#pragma comment ( lib, "xaudio2.lib" )

#include "BufferedBuffer.h"

class XAudio2AudioPlayer : public AbstractAudioPlayer1<IXAudio2>, IXAudio2VoiceCallback
{
private:
	~XAudio2AudioPlayer () { sourceVoice->DestroyVoice (); }

public:
	virtual HRESULT STDMETHODCALLTYPE Initialize ( AbstractAudioSampleReader * audioReader,
		IXAudio2 * xaudio )
	{
		HRESULT hr;

		QueryPerformanceFrequency ( &performanceFrequency );

		this->audioReader = audioReader;
		bufferedBuffer = new BufferedBuffer ( audioReader );

		AudioInformation audioInfo;
		audioReader->GetAudioInfo ( &audioInfo );

		WAVEFORMATEX pwfx;
		pwfx.cbSize = sizeof ( WAVEFORMATEX );
		pwfx.wFormatTag = WAVE_FORMAT_PCM;
		pwfx.wBitsPerSample = audioInfo.audioBitsPerSample;
		pwfx.nChannels = audioInfo.audioChannels;
		pwfx.nSamplesPerSec = audioInfo.audioSampleRate;
		pwfx.nBlockAlign = pwfx.nChannels * pwfx.wBitsPerSample / 8;
		pwfx.nAvgBytesPerSec = pwfx.nBlockAlign * pwfx.nSamplesPerSec;
		if ( FAILED ( hr = xaudio->CreateSourceVoice ( &sourceVoice, &pwfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, ( IXAudio2VoiceCallback* ) this ) ) )
			return hr;

		bytesPerFrame = audioInfo.audioSampleRate;// * audioInfo.audioChannels * ( audioInfo.audioBitsPerSample / 8 );

		isPlaying = false;

		return S_OK;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE Play ()
	{
		if ( isPlaying )
			return E_FAIL;

		meetEOF = false;
		firstSampleTime = -1;

		Process ();
		sourceVoice->Start ();
		isPlaying = true;
		QueryPerformanceCounter ( &firstStartTime );
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE Stop ()
	{
		meetEOF = true;
		sourceVoice->Stop ();
		isPlaying = false;
		return S_OK;
	}

private:
	void Process ()
	{
		if ( meetEOF ) return;

		SampleReaderState state;
		CComPtr<AbstractSample> sample;
		if ( FAILED ( audioReader->GetSample ( &state, ( NanoSecTime * ) &readPos,
			&sample/*, bytesPerFrame*/ ) ) )
		{
			printfmt ( L"ERROR on AbstractSample::GetSample ()\n" );
			return;
		}

		if ( state == SRS_EOF )
		{
			XAUDIO2_BUFFER xBuffer = {};
			xBuffer.Flags = XAUDIO2_END_OF_STREAM;
			if ( FAILED ( sourceVoice->SubmitSourceBuffer ( &xBuffer ) ) )
				printfmt ( L"ERROR on IXAudio2SourceVoice::SubmitSourceBuffer ()\n" );

			meetEOF = true;
			isPlaying = false;
			return;
		}

		BYTE * buffer;
		DWORD bufferLength;
		sample->Lock ( &buffer, &bufferLength );

		XAUDIO2_BUFFER xBuffer = {};
		xBuffer.pAudioData = buffer;
		xBuffer.AudioBytes = bufferLength;
		if ( FAILED ( sourceVoice->SubmitSourceBuffer ( &xBuffer ) ) )
			printfmt ( L"ERROR on IXAudio2SourceVoice::SubmitSourceBuffer ()\n" );

		if ( firstSampleTime == -1 )
			firstSampleTime = readPos;

		sample->Unlock ();
	}

public:
	virtual HRESULT STDMETHODCALLTYPE GetPosition ( NanoSecTime * pos )
	{
		if ( !isPlaying )
		{
			*pos = 0;
			return S_OK;
		}

		LARGE_INTEGER currentTime;
		QueryPerformanceCounter ( &currentTime );

		*pos = firstSampleTime + ( ( currentTime.QuadPart - firstStartTime.QuadPart ) * 10000000 / performanceFrequency.QuadPart );

		return S_OK;
	}

public:
	STDMETHOD_ ( void, OnVoiceProcessingPassStart ) ( THIS_ UINT32 BytesRequired ) { }
	STDMETHOD_ ( void, OnVoiceProcessingPassEnd ) ( THIS ) { }
	STDMETHOD_ ( void, OnStreamEnd ) ( THIS ) { }
	STDMETHOD_ ( void, OnBufferStart ) ( THIS_ void* pBufferContext ) { }
	STDMETHOD_ ( void, OnBufferEnd ) ( THIS_ void* pBufferContext )
	{
		Process ();
	}
	STDMETHOD_ ( void, OnLoopEnd ) ( THIS_ void* pBufferContext ) { }
	STDMETHOD_ ( void, OnVoiceError ) ( THIS_ void* pBufferContext, HRESULT Error ) { }

private:
	IXAudio2SourceVoice * sourceVoice;

	CComPtr<AbstractAudioSampleReader> audioReader;
	CComPtr<BufferedBuffer> bufferedBuffer;
	LONGLONG readPos;
	LONGLONG playingPos;
	bool meetEOF;

	LARGE_INTEGER performanceFrequency;
	LARGE_INTEGER firstStartTime;
	LONGLONG firstSampleTime;

	int bytesPerFrame;
	bool isPlaying;
};