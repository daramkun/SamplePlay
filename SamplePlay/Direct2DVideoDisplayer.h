#pragma once

#include "Interface.h"

#include "Window.h"

#include <d3d11.h>
#include <dxgi.h>
#include <d2d1.h>
#include <d2d1helper.h>

#pragma comment ( lib, "d3d11.lib" )
#pragma comment ( lib, "dxgi.lib" )
#pragma comment ( lib, "d2d1.lib" )

class Direct2DVideoDisplayer : public AbstractVideoDisplayer
{
public:
	virtual HRESULT STDMETHODCALLTYPE Initialize ( AbstractVideoSampleReader * videoReader,
		AbstractWindow * window, AbstractAudioPlayer * audioPlayer )
	{
		this->videoReader = videoReader;
		this->window = window;
		this->audioPlayer = audioPlayer;

		videoReader->GetVideoInfo ( &videoInfo );

		CComPtr<ID2D1Factory> d2dFactory;
		if ( FAILED ( D2D1CreateFactory ( D2D1_FACTORY_TYPE_MULTI_THREADED, &d2dFactory ) ) )
			return E_FAIL;

		D2D1_RENDER_TARGET_PROPERTIES renderTargetProps;
		renderTargetProps.pixelFormat.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		renderTargetProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
		renderTargetProps.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;
		renderTargetProps.type = D2D1_RENDER_TARGET_TYPE_HARDWARE;
		renderTargetProps.usage = D2D1_RENDER_TARGET_USAGE_NONE;
		renderTargetProps.dpiX = 96;
		renderTargetProps.dpiY = 96;

		hWnd = GetHandleFromWindow ( static_cast< WindowsWindow* >( window ) );

		RECT clientRect;
		GetClientRect ( hWnd, &clientRect );

		UINT width = clientRect.right - clientRect.left,
			height = clientRect.bottom - clientRect.top;

		D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRenderTargetProps;
		hwndRenderTargetProps.hwnd = hWnd;
		hwndRenderTargetProps.pixelSize.width = width;
		hwndRenderTargetProps.pixelSize.height = height;
		hwndRenderTargetProps.presentOptions = D2D1_PRESENT_OPTIONS_IMMEDIATELY;

		if ( FAILED ( d2dFactory->CreateHwndRenderTarget ( &renderTargetProps,
			&hwndRenderTargetProps, ( ID2D1HwndRenderTarget** ) &d2dRenderTarget ) ) )
			return E_FAIL;

		if ( FAILED ( d2dRenderTarget->CreateBitmap ( D2D1::SizeU ( videoInfo.videoWidth, videoInfo.videoHeight ),
			D2D1::BitmapProperties ( D2D1::PixelFormat ( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED ) ), &bitmap ) ) )
			return E_FAIL;

		return S_OK;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE DrawSample ()
	{
		if ( readSample == nullptr )
			if ( FAILED ( videoReader->GetSample ( &readState, &readTime, &readSample ) ) )
				return E_FAIL;

		NanoSecTime audioPos;
		audioPlayer->GetPosition ( &audioPos );
		if ( readTime <= audioPos )
		{
			printfmt ( TEXT ( "Playing position: %lldms(video: %lldms)\n" ),
				audioPos / 10000, readTime / 10000 );

			BYTE * buf;
			DWORD currentLength;
			if ( FAILED ( readSample->Lock ( &buf, &currentLength ) ) )
			{
				readSample.Release ();
				return E_FAIL;
			}

			if ( currentLength != videoInfo.videoWidth * 4 * videoInfo.videoHeight )
			{
				readSample.Release ();
				return E_FAIL;
			}

			if ( FAILED ( bitmap->CopyFromMemory ( nullptr, buf, videoInfo.videoWidth * 4 ) ) )
			{
				readSample.Release ();
				return E_FAIL;
			}

			readSample->Unlock ();

			RECT clientRect;
			GetClientRect ( hWnd, &clientRect );

			d2dRenderTarget->BeginDraw ();
			d2dRenderTarget->Clear ( D2D1::ColorF ( 0, 0, 0, 1 ) );
			d2dRenderTarget->DrawBitmap ( bitmap,
				D2D1::RectF (
					0,
					0,
					( float ) ( clientRect.right - clientRect.left ),
					( float ) ( clientRect.bottom - clientRect.top )
				),
				1, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
				D2D1::RectF (
					0,
					0, 
					( float ) videoInfo.videoWidth,
					( float ) videoInfo.videoHeight
				)
			);
			d2dRenderTarget->EndDraw ();

			readSample.Release ();
		}

		return S_OK;
	}

private:
	CComPtr<AbstractVideoSampleReader> videoReader;
	CComPtr<AbstractWindow> window;
	CComPtr<AbstractAudioPlayer> audioPlayer;

	HWND hWnd;
	VideoInformation videoInfo;

	CComPtr<ID2D1RenderTarget> d2dRenderTarget;
	CComPtr<ID2D1Bitmap> bitmap;

	SampleReaderState readState;
	CComPtr<AbstractSample> readSample;
	NanoSecTime readTime;
};