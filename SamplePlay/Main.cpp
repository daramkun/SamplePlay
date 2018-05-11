#include <Windows.h>
#include <VersionHelpers.h>

#include <atlbase.h>

#include "Interface.h"
#include "Window.h"
#include "MediaFoundationSampleReader.h"
#include "BufferedBuffer.h"
#include "WASAPIAudioPlayer.h"
#include "XAudio2AudioPlayer.h"
#include "Direct2DVideoDisplayer.h"

#include <vector>
#include <algorithm>

#pragma region Initialize-Uninitialize Pair class
class COM
{
public: COM () { CoInitialize ( nullptr ); }
		~COM () { CoUninitialize (); }
};
class MF
{
public: MF () { MFStartup ( MF_VERSION ); }
		~MF () { MFShutdown (); }
};
class AudioClient
{
public:
	AudioClient ()
	{
		CoCreateInstance ( __uuidof ( MMDeviceEnumerator ), nullptr, CLSCTX_ALL,
			__uuidof( IMMDeviceEnumerator ), ( void ** ) &devEnum );
	}

public:
	operator IMMDevice* ( )
	{
		IMMDevice * dev;
		devEnum->GetDefaultAudioEndpoint ( eRender, eConsole, &dev );
		return dev;
	}

private:
	CComPtr<IMMDeviceEnumerator> devEnum;
};
class XAudio2
{
public: 
	XAudio2 ()
	{
		XAudio2Create ( &xa );
		xa->CreateMasteringVoice ( &masteringVoice );
	}
	~XAudio2 ()
	{
		masteringVoice->DestroyVoice ();
	}

public:
	operator IXAudio2* ( ) { return xa; }

private:
	CComPtr<IXAudio2> xa;
	IXAudio2MasteringVoice * masteringVoice;
};
#pragma endregion

int WINAPI wWinMain ( HINSTANCE hInstance, HINSTANCE, LPWSTR lpszCmdLine, int nCmdShow )
{
	COM com;
	MF mf;
	AudioClient ac;
	XAudio2 xa;

	const LPCTSTR VIDEOFILEPATH = TEXT ( "F:\\새 폴더\\새 폴더\\오버워치 단편 애니메이션   “일어나요!”.mp4" );

	CComPtr<AbstractWindow> window = new WindowsWindow ();
	if ( FAILED ( window->Initialize ( TEXT ( "Sample Play" ), 1280, 720 ) ) )
		return -1;
	window->Show ();

	CComPtr<AbstractSampleReader> sourceReader = new MFSampleReader ();
	if ( FAILED ( sourceReader->Initialize ( VIDEOFILEPATH ) ) )
		return -2;
	CComPtr<AbstractVideoSampleReader> videoReader;
	sourceReader->GetVideoSampleReader ( &videoReader );
	VideoInformation videoInfo;
	videoReader->GetVideoInfo ( &videoInfo );

	CComPtr<AbstractAudioSampleReader> audioReader;
	sourceReader->GetAudioSampleReader ( &audioReader );
	AudioInformation audioInfo;
	audioReader->GetAudioInfo ( &audioInfo );

	CComPtr<AbstractAudioPlayer1<IXAudio2>> audioPlayer = new XAudio2AudioPlayer ();
	if ( FAILED ( audioPlayer->Initialize ( audioReader, xa ) ) )
		return -6;
	CComPtr<AbstractVideoDisplayer> videoDisplayer = new Direct2DVideoDisplayer ();
	videoDisplayer->Initialize ( videoReader, window, audioPlayer );

	audioPlayer->Play ();

	MSG msg;
	while ( true )
	{
		if ( PeekMessage ( &msg, nullptr, 0, 0, PM_NOREMOVE ) )
		{
			if ( !GetMessage ( &msg, nullptr, 0, 0 ) )
				break;
			TranslateMessage ( &msg );
			DispatchMessage ( &msg );
		}
		else
		{
			videoDisplayer->DrawSample ();

			Sleep ( 1 );
		}
	}

	return 0;
}