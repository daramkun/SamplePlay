#pragma once

#include "Interface.h"

#include <vector>

class BufferedBufferSample : public AbstractSample
{
public:
	BufferedBufferSample ( const std::vector<BYTE> & buffer, NanoSecTime time, NanoSecTime duration )
		: buffer ( buffer ), readTime ( time ), duration ( duration )
	{ }

public:
	virtual HRESULT STDMETHODCALLTYPE GetSampleTime ( NanoSecTime * time )
	{
		*time = readTime;
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE GetSampleDuration ( NanoSecTime * time )
	{
		*time = duration;
		return S_OK;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE Lock ( BYTE ** buffer, DWORD * length )
	{
		if ( this->buffer.size () == 0 )
		{
			*buffer = nullptr;
			*length = 0;
			return E_FAIL;
		}
		*buffer = &*this->buffer.begin ();
		*length = this->buffer.size ();
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE Unlock () { return S_OK; }

private:
	std::vector<BYTE> buffer;
	NanoSecTime readTime, duration;
};

class BufferedBuffer : public AbstractUnknown
{
public:
	BufferedBuffer ( AbstractAudioSampleReader * reader, UINT bufferCapacity = 2097152 )
		: audioReader ( reader ), bufferCapacity ( bufferCapacity ), readTime ( 0 )
	{
		reader->GetAudioInfo ( &audioInfo );
	}

public:
	virtual HRESULT STDMETHODCALLTYPE GetSample ( SampleReaderState * state,
		NanoSecTime * readTime, AbstractSample ** sample, UINT requestReadingSize )
	{
		bool eof = false;
		if ( requestReadingSize > buffer.size () )
		{
			LONGLONG readedTime = 0x1fffffffffffffff;

			while ( bufferCapacity > buffer.size () )
			{
				CComPtr<AbstractSample> as;
				NanoSecTime tempReadTime;
				if ( FAILED ( audioReader->GetSample ( state, &tempReadTime, &as ) ) )
					continue;

				if ( *state == SRS_EOF )
				{
					eof = true;
					break;
				}

				BYTE * readData;
				DWORD byteSize;
				if ( FAILED ( as->Lock ( &readData, &byteSize ) ) )
					return E_FAIL;

				buffer.insert ( buffer.end (), readData, readData + byteSize );

				as->Unlock ();

				if ( this->readTime < 0 )
					this->readTime = tempReadTime;
			}
		}

		if ( !eof && buffer.size () == 0 )
		{
			*state = SRS_UNKNOWNFAIL;
			return S_OK;
		}

		if ( eof && buffer.size () == 0 )
		{
			*state = SRS_EOF;
			return S_OK;
		}
		else *state = SRS_NONE;

		size_t sampleSize = min ( requestReadingSize, buffer.size () );

		*readTime = this->readTime;
		std::vector<BYTE> subvec ( buffer.begin (), buffer.begin () + sampleSize );
		NanoSecTime duration ( sampleSize * 10000000 / audioInfo.getByteRate () );
		*sample = new BufferedBufferSample ( subvec, NanoSecTime ( this->readTime ), duration );
		buffer.erase ( buffer.begin (), buffer.begin () + sampleSize );
		this->readTime += duration;

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Flush ()
	{
		buffer.clear ();
		this->readTime = -1;
		return S_OK;
	}

private:
	CComPtr<AbstractAudioSampleReader> audioReader;
	AudioInformation audioInfo;
	std::vector<BYTE> buffer;
	UINT bufferCapacity;
	LONGLONG readTime;
};