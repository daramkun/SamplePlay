#pragma once

#include <Windows.h>
#include <mmdeviceapi.h>
#include <functional>

#include <atlbase.h>

#pragma region Debug Functions
#include <stdarg.h>
void print ( LPCTSTR lpszText )
{
	::OutputDebugString ( lpszText );
}
int printfmt ( LPCTSTR lpszFormat, ... )
{
	va_list args;
	va_start ( args, lpszFormat );
	int nBuf;
	TCHAR szBuffer [ 1024 ]; // get rid of this hard-coded buffer
#if _UNICODE
	nBuf = _vsnwprintf_s ( szBuffer, 1023, lpszFormat, args );
#else
	nBuf = _vsnprintf_s ( szBuffer, 1023, lpszFormat, args );
#endif
	::OutputDebugString ( szBuffer );
	va_end ( args );

	return nBuf;
}
#pragma endregion

enum PerformanceCounterUnit
{
	PCU_MILLISEC = 1000,
	PCU_NANOSEC = 10000000,
};

struct PerformanceCounter
{
public:
	PerformanceCounter ( PerformanceCounterUnit pcu = PCU_NANOSEC )
		: pcu ( pcu ) { QueryPerformanceFrequency ( &frequency ); }

public:
	LONGLONG GetCounter ()
	{
		LARGE_INTEGER counter;
		QueryPerformanceCounter ( &counter );
		return counter.QuadPart * pcu / frequency.QuadPart;
	}

private:
	LARGE_INTEGER frequency;
	PerformanceCounterUnit pcu;
};

struct NanoSecTime
{
public:
	NanoSecTime ( LONGLONG time = 0 ) : time ( time ) { }

public:
	UINT GetMilliSec () { return time / 10000; }
	operator LONGLONG () const { return time; }

private:
	LONGLONG time;
};

struct MilliSecTime
{
	MilliSecTime ( UINT time = 0 ) : time ( time ) { }

public:
	LONGLONG GetNanoSec () { return time * 10000; }
	operator UINT () const { return time; }

private:
	UINT time;
};

class AbstractUnknown : public IUnknown
{
public:
	AbstractUnknown () : refCount ( 1 ) { }
	virtual ~AbstractUnknown () { }

public:
	virtual HRESULT STDMETHODCALLTYPE QueryInterface ( REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject )
	{
		if ( riid == __uuidof ( IUnknown ) )
		{
			*ppvObject = this;
			AddRef ();
		}
		else return E_FAIL;

		return S_OK;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef ( void ) { InterlockedIncrement ( &refCount ); return refCount; }
	virtual ULONG STDMETHODCALLTYPE Release ( void )
	{
		ULONG d = InterlockedDecrement ( &refCount );
		if ( d == 0 )
			delete this;
		return d;
	}

private:
	ULONG refCount;
};

class AbstractWindow : public AbstractUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE Initialize ( LPCTSTR title, UINT width, UINT height ) PURE;
	virtual HRESULT STDMETHODCALLTYPE Show () PURE;

public:
	virtual HRESULT STDMETHODCALLTYPE Run ( std::function<void ()> & run ) PURE;

public:
	virtual HRESULT STDMETHODCALLTYPE GetClientSize ( UINT * width, UINT * height ) PURE;
};

struct VideoInformation
{
	UINT videoWidth;
	UINT videoHeight;
	UINT frameRateNumerator;
	UINT frameRateDenominator;
};

struct AudioInformation
{
	UINT audioChannels, audioSampleRate, audioBitsPerSample;

	UINT getByteRate () { return audioChannels * audioSampleRate * ( audioBitsPerSample / 8 ); }
};

class AbstractVideoSampleReader;
class AbstractAudioSampleReader;

class AbstractSampleReader : public AbstractUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE Initialize ( LPCTSTR filename ) PURE;

public:
	virtual HRESULT STDMETHODCALLTYPE GetDuration ( NanoSecTime * duration ) PURE;
	virtual HRESULT STDMETHODCALLTYPE SetSamplingOffset ( const NanoSecTime & time ) PURE;

public:
	virtual HRESULT STDMETHODCALLTYPE GetVideoSampleReader ( AbstractVideoSampleReader ** reader ) PURE;
	virtual HRESULT STDMETHODCALLTYPE GetAudioSampleReader ( AbstractAudioSampleReader ** reader ) PURE;
};

enum SampleReaderState
{
	SRS_NONE,
	SRS_UNKNOWNFAIL,
	SRS_EOF,
};

class AbstractSample : public AbstractUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE GetSampleTime ( NanoSecTime * time ) PURE;
	virtual HRESULT STDMETHODCALLTYPE GetSampleDuration ( NanoSecTime * time ) PURE;

public:
	virtual HRESULT STDMETHODCALLTYPE Lock ( BYTE ** buffer, DWORD * length ) PURE;
	virtual HRESULT STDMETHODCALLTYPE Unlock () PURE;
};

class AbstractVideoSampleReader : public AbstractUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE GetVideoInfo ( VideoInformation * info ) PURE;

public:
	virtual HRESULT STDMETHODCALLTYPE GetSample ( SampleReaderState * state, NanoSecTime * readTime, AbstractSample ** sample ) PURE;
};

class AbstractAudioSampleReader : public AbstractUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE GetAudioInfo ( AudioInformation * info ) PURE;

public:
	virtual HRESULT STDMETHODCALLTYPE GetSample ( SampleReaderState * state, NanoSecTime * readTime, AbstractSample ** sample ) PURE;
};

class AbstractAudioPlayer : public AbstractUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE Play () PURE;
	virtual HRESULT STDMETHODCALLTYPE Stop () PURE;
	
public:
	virtual HRESULT STDMETHODCALLTYPE GetPosition ( NanoSecTime * pos ) PURE;
};

template<typename T>
class AbstractAudioPlayer1 : public AbstractAudioPlayer
{
public:
	virtual HRESULT STDMETHODCALLTYPE Initialize ( AbstractAudioSampleReader * audioReader, T * t ) PURE;
};

class AbstractVideoDisplayer : public AbstractUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE Initialize ( AbstractVideoSampleReader * videoReader,
		AbstractWindow * window, AbstractAudioPlayer * audioPlayer ) PURE;

public:
	virtual HRESULT STDMETHODCALLTYPE DrawSample () PURE;
};