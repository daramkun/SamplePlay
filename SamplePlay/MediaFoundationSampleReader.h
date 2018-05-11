#pragma once

#include "Interface.h"

#include <atlbase.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#pragma comment ( lib, "mf.lib" )
#pragma comment ( lib, "mfplat.lib" )
#pragma comment ( lib, "mfuuid.lib" )
#pragma comment ( lib, "mfreadwrite.lib" )

#include <Propvarutil.h>
#pragma comment ( lib, "Propsys.lib" )

class MFSample : public AbstractSample
{
public:
	MFSample ( IMFSample * sample )
		: sample ( sample )
	{

	}

public:
	virtual HRESULT STDMETHODCALLTYPE GetSampleTime ( NanoSecTime * time )
	{
		return sample->GetSampleTime ( ( LONGLONG * ) &time );
	}
	virtual HRESULT STDMETHODCALLTYPE GetSampleDuration ( NanoSecTime * time )
	{
		return sample->GetSampleDuration ( ( LONGLONG * ) &time );
	}

public:
	virtual HRESULT STDMETHODCALLTYPE Lock ( BYTE ** buffer, DWORD * length )
	{
		HRESULT hr;
		if ( FAILED ( hr = sample->ConvertToContiguousBuffer ( &this->buffer ) ) )
			return hr;

		return this->buffer->Lock ( buffer, nullptr, length );
	}
	virtual HRESULT STDMETHODCALLTYPE Unlock ()
	{
		if ( buffer == nullptr )
			return E_FAIL;

		HRESULT hr;
		if ( FAILED ( hr = buffer->Unlock () ) )
			return hr;

		buffer.Release ();

		return S_OK;
	}

private:
	CComPtr<IMFSample> sample;
	CComPtr<IMFMediaBuffer> buffer;
};

class MFVideoSampleReader : public AbstractVideoSampleReader
{
public:
	MFVideoSampleReader ( IMFSourceReader * sourceReader, DWORD videoStreamIndex, const VideoInformation & videoInfo )
		: sourceReader ( sourceReader ), videoStreamIndex ( videoStreamIndex ), videoInfo ( videoInfo )
	{ }

public:
	virtual HRESULT STDMETHODCALLTYPE GetVideoInfo ( VideoInformation * info )
	{
		*info = videoInfo;
		return S_OK;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE GetSample ( SampleReaderState * state, NanoSecTime * readTime, AbstractSample ** sample )
	{
		DWORD flags;
		CComPtr<IMFSample> mfSample;
		HRESULT hr;
		if ( FAILED ( hr = sourceReader->ReadSample ( videoStreamIndex, 0, nullptr, &flags,
			( LONGLONG* ) readTime, &mfSample ) ) )
		{
			*state = SRS_UNKNOWNFAIL;
			return hr;
		}
		if ( flags & MF_SOURCE_READERF_ENDOFSTREAM )
			*state = SRS_EOF;

		*sample = new MFSample ( mfSample );

		return S_OK;
	}

private:
	CComPtr<IMFSourceReader> sourceReader;
	DWORD videoStreamIndex;
	VideoInformation videoInfo;
};

class MFAudioSampleReader : public AbstractAudioSampleReader
{
public:
	MFAudioSampleReader ( IMFSourceReader * sourceReader, DWORD audioStreamIndex, const AudioInformation & audioInfo )
		: sourceReader ( sourceReader ), audioStreamIndex ( audioStreamIndex ), audioInfo ( audioInfo )
	{ }

public:
	virtual HRESULT STDMETHODCALLTYPE GetAudioInfo ( AudioInformation * info )
	{
		*info = audioInfo;
		return S_OK;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE GetSample ( SampleReaderState * state, NanoSecTime * readTime, AbstractSample ** sample )
	{
		DWORD flags;
		CComPtr<IMFSample> mfSample;
		HRESULT hr;
		if ( FAILED ( hr = sourceReader->ReadSample ( audioStreamIndex, 0, nullptr, &flags,
			( LONGLONG* ) readTime, &mfSample ) ) )
		{
			*state = SRS_UNKNOWNFAIL;
			return hr;
		}
		if ( flags & MF_SOURCE_READERF_ENDOFSTREAM )
			*state = SRS_EOF;

		*sample = new MFSample ( mfSample );

		return S_OK;
	}

private:
	CComPtr<IMFSourceReader> sourceReader;
	DWORD audioStreamIndex;
	AudioInformation audioInfo;
};

class MFSampleReader : public AbstractSampleReader
{
public:
	virtual HRESULT STDMETHODCALLTYPE Initialize ( LPCTSTR filename )
	{
		CComPtr<IMFAttributes> attribute;
		if ( FAILED ( MFCreateAttributes ( &attribute, 0 ) ) )
			return false;

		attribute->SetUINT32 ( MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE );
		if ( IsWindows8OrGreater () )
			attribute->SetUINT32 ( MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE );

		if ( FAILED ( MFCreateSourceReaderFromURL ( filename, attribute, &sourceReader ) ) )
			return false;

		DWORD videoStreamIndex, audioStreamIndex;

		DWORD temp1; LONGLONG temp2; IMFSample * temp3;
		if ( SUCCEEDED ( sourceReader->ReadSample ( MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			MF_SOURCE_READER_CONTROLF_DRAIN, &videoStreamIndex, &temp1, &temp2, &temp3 ) ) )
		{
			CComPtr<IMFMediaType> inputVideoMediaType;
			sourceReader->GetNativeMediaType ( videoStreamIndex, 0, &inputVideoMediaType );

			VideoInformation videoInfo;
			MFGetAttributeSize ( inputVideoMediaType, MF_MT_FRAME_SIZE, &videoInfo.videoWidth, &videoInfo.videoHeight );
			MFGetAttributeRatio ( inputVideoMediaType, MF_MT_FRAME_RATE, &videoInfo.frameRateNumerator, &videoInfo.frameRateDenominator );

			CComPtr<IMFMediaType> outputVideoMediaType;
			MFCreateMediaType ( &outputVideoMediaType );

			outputVideoMediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
			outputVideoMediaType->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_RGB32 );
			outputVideoMediaType->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
			MFSetAttributeSize ( outputVideoMediaType, MF_MT_FRAME_SIZE, videoInfo.videoWidth, videoInfo.videoHeight );
			MFSetAttributeRatio ( outputVideoMediaType, MF_MT_FRAME_RATE, videoInfo.frameRateNumerator, videoInfo.frameRateDenominator );
			MFSetAttributeRatio ( outputVideoMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

			sourceReader->SetCurrentMediaType ( videoStreamIndex, nullptr, outputVideoMediaType );
		
			*&videoSampleReader = new MFVideoSampleReader ( sourceReader, videoStreamIndex, videoInfo );
		}
		if ( SUCCEEDED ( sourceReader->ReadSample ( MF_SOURCE_READER_FIRST_AUDIO_STREAM,
			MF_SOURCE_READER_CONTROLF_DRAIN, &audioStreamIndex, &temp1, &temp2, &temp3 ) ) )
		{
			CComPtr<IMFMediaType> inputAudioMediaType;
			sourceReader->GetNativeMediaType ( audioStreamIndex, 0, &inputAudioMediaType );

			AudioInformation audioInfo;
			inputAudioMediaType->GetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, &audioInfo.audioChannels );
			inputAudioMediaType->GetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, &audioInfo.audioSampleRate );
			inputAudioMediaType->GetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, &audioInfo.audioBitsPerSample );

			CComPtr<IMFMediaType> outputAudioMediaType;
			MFCreateMediaType ( &outputAudioMediaType );

			outputAudioMediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
			outputAudioMediaType->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_PCM );
			outputAudioMediaType->SetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, audioInfo.audioChannels );
			outputAudioMediaType->SetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, audioInfo.audioSampleRate );
			outputAudioMediaType->SetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, audioInfo.audioBitsPerSample );

			sourceReader->SetCurrentMediaType ( audioStreamIndex, nullptr, outputAudioMediaType );

			*&audioSampleReader = new MFAudioSampleReader ( sourceReader, audioStreamIndex, audioInfo );
		}

		return S_OK;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE GetDuration ( NanoSecTime * duration )
	{
		PROPVARIANT var;
		HRESULT hr = sourceReader->GetPresentationAttribute ( MF_SOURCE_READER_MEDIASOURCE,
			MF_PD_DURATION, &var );
		if ( FAILED ( hr ) ) return hr;

		PropVariantToInt64 ( var, ( LONGLONG* ) duration );
		PropVariantClear ( &var );

		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE SetSamplingOffset ( const NanoSecTime & time )
	{
		PROPVARIANT pos;
		pos.vt = VT_I8;
		pos.hVal.QuadPart = ( LONGLONG ) time;
		sourceReader->SetCurrentPosition ( GUID_NULL, pos );

		return S_OK;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE GetVideoSampleReader ( AbstractVideoSampleReader ** reader )
	{
		if ( videoSampleReader == nullptr )
			return E_NOT_SET;
		*reader = videoSampleReader;
		( *reader )->AddRef ();
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE GetAudioSampleReader ( AbstractAudioSampleReader ** reader )
	{
		if ( audioSampleReader == nullptr )
			return E_NOT_SET;
		*reader = audioSampleReader;
		( *reader )->AddRef ();
		return S_OK;
	}

private:
	CComPtr<IMFSourceReader> sourceReader;
	CComPtr<AbstractVideoSampleReader> videoSampleReader;
	CComPtr<AbstractAudioSampleReader> audioSampleReader;
};