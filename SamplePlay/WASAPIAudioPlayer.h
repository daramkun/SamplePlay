#pragma once

#include "Interface.h"

#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#include <Wmcodecdsp.h>
#include <dmo.h>
#pragma comment ( lib, "dmoguids.lib" )

#include "BufferedBuffer.h"

class WASAPIAudioPlayer : public AbstractAudioPlayer1<IMMDevice>
{
public:
	virtual HRESULT STDMETHODCALLTYPE Initialize ( AbstractAudioSampleReader * audioReader,
		IMMDevice * mmDevice )
	{
		HRESULT hr;

		QueryPerformanceFrequency ( &performanceFrequency );

		this->audioReader = audioReader;
		bufferedBuffer = new BufferedBuffer ( audioReader );

		AudioInformation audioInfo;
		audioReader->GetAudioInfo ( &audioInfo );

		if ( FAILED ( mmDevice->Activate ( __uuidof ( IAudioClient ), CLSCTX_ALL,
			NULL, ( void** ) &audioClient ) ) )
			return false;

		WAVEFORMATEX * pwfx;
		audioClient->GetMixFormat ( &pwfx );
		switch ( pwfx->wFormatTag )
		{
			case WAVE_FORMAT_PCM:
			case WAVE_FORMAT_IEEE_FLOAT:
				pwfx->wFormatTag = WAVE_FORMAT_PCM;
				pwfx->wBitsPerSample = audioInfo.audioBitsPerSample;
				pwfx->nSamplesPerSec = audioInfo.audioSampleRate;
				pwfx->nChannels = audioInfo.audioChannels;
				pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
				pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
				break;

			case WAVE_FORMAT_EXTENSIBLE:
				{
					PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>( pwfx );
					if ( IsEqualGUID ( KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat ) )
					{
						pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
						pEx->Samples.wValidBitsPerSample = audioInfo.audioBitsPerSample;
						//pEx->Format.wFormatTag = WAVE_FORMAT_PCM;
						pEx->Format.wBitsPerSample = audioInfo.audioBitsPerSample;
						pEx->Format.nSamplesPerSec = audioInfo.audioSampleRate;
						pEx->Format.nChannels = audioInfo.audioChannels;
						pEx->Format.nBlockAlign = audioInfo.audioChannels * audioInfo.audioBitsPerSample / 8;
						pEx->Format.nAvgBytesPerSec = pEx->Format.nBlockAlign * audioInfo.audioSampleRate;
						pEx->dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
					}
				}
				break;
		}

		WAVEFORMATEX * closest;
		if ( FAILED ( hr = audioClient->IsFormatSupported ( AUDCLNT_SHAREMODE_SHARED, pwfx, &closest ) ) )
			return hr;

		REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
		if ( FAILED ( hr = audioClient->Initialize ( AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			hnsRequestedDuration, 0, closest ? closest : pwfx, NULL ) ) )
			return hr;

		if ( closest )
		{
			// DMO Resampling

			CoTaskMemFree ( closest );
		}

		hEvent = CreateEvent ( nullptr, false, false, nullptr );
		audioClient->SetEventHandle ( hEvent );
		audioClient->GetBufferSize ( &audioClientBufferSize );

		bytesPerFrame = audioInfo.audioChannels * audioInfo.audioBitsPerSample / 8;

		audioClient->GetService ( __uuidof( IAudioRenderClient ), ( void ** ) &audioRenderClient );

		return S_OK;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE Play ()
	{
		if ( thread )
			return E_FAIL;

		meetEOF = false;
		firstSampleTime = -1;

		Process ( false );
		audioClient->Start ();
		thread = CreateThread ( nullptr, 0, ThreadMethod, this, 0, nullptr );
		QueryPerformanceCounter ( &firstStartTime );
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE Stop ()
	{
		meetEOF = true;
		audioClient->Stop ();
		thread = NULL;
		return S_OK;
	}

private:
	void Process ( bool wait = true )
	{
		if ( meetEOF ) return;

		if ( wait )
			WaitForSingleObject ( hEvent, INFINITE );

		UINT paddingFrame;
		audioClient->GetCurrentPadding ( &paddingFrame );

		UINT32 availableFrames = audioClientBufferSize - paddingFrame;
		int actualSize = availableFrames * bytesPerFrame;

		SampleReaderState state;
		CComPtr<AbstractSample> sample;
		if ( FAILED ( bufferedBuffer->GetSample ( &state, ( NanoSecTime * ) &readPos,
			&sample, actualSize ) ) )
			return;

		if ( state == SRS_EOF )
		{
			meetEOF = true;
			thread = NULL;
			return;
		}

		BYTE * data;
		HRESULT hr;
		if ( FAILED ( hr = audioRenderClient->GetBuffer ( availableFrames, &data ) ) )
			return;

		if ( firstSampleTime == -1 )
			firstSampleTime = readPos;

		BYTE * buffer;
		DWORD bufferLength;
		sample->Lock ( &buffer, &bufferLength );

		memcpy ( data, &*buffer, bufferLength );

		sample->Unlock ();

		if ( FAILED ( hr = audioRenderClient->ReleaseBuffer ( bufferLength / bytesPerFrame, 0 ) ) )
			return;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE GetPosition ( NanoSecTime * pos )
	{
		if ( thread == NULL )
		{
			*pos = 0;
			return S_OK;
		}

		LARGE_INTEGER currentTime;
		QueryPerformanceCounter ( &currentTime );

		*pos = firstSampleTime + ( ( currentTime.QuadPart - firstStartTime.QuadPart ) * 10000000 / performanceFrequency.QuadPart );

		return S_OK;
	}

private:
	static DWORD WINAPI ThreadMethod ( LPVOID lpv )
	{
		WASAPIAudioPlayer * player = ( WASAPIAudioPlayer * ) lpv;
		while ( !player->meetEOF )
			player->Process ( true );
		return 0;
	}

private:
	IAudioClient * audioClient;
	CComPtr<IAudioRenderClient> audioRenderClient;
	HANDLE hEvent;
	UINT audioClientBufferSize;

	CComPtr<IWMResamplerProps> resampler;

	CComPtr<AbstractAudioSampleReader> audioReader;
	CComPtr<BufferedBuffer> bufferedBuffer;
	LONGLONG readPos;
	LONGLONG playingPos;
	bool meetEOF;

	LARGE_INTEGER performanceFrequency;
	LARGE_INTEGER firstStartTime;
	LONGLONG firstSampleTime;

	int bytesPerFrame;

	HANDLE thread;
};