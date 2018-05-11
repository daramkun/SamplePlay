#pragma once

#include "Interface.h"

#define WINDOWSWINDOW_CLASSNAME								TEXT ( "WindowsWindow" )

class WindowsWindow : public AbstractWindow
{
public:
	virtual HRESULT STDMETHODCALLTYPE Initialize ( LPCTSTR title, UINT width, UINT height )
	{
		WNDCLASSEX wndClass = { 0, };
		wndClass.cbSize = sizeof ( WNDCLASSEX );
		wndClass.hbrBackground = ( HBRUSH ) GetStockObject ( WHITE_BRUSH );
		wndClass.hIcon = LoadIcon ( nullptr, IDI_APPLICATION );
		wndClass.hCursor = LoadCursor ( nullptr, IDC_ARROW );
		wndClass.hInstance = GetModuleHandle ( nullptr );
		wndClass.lpfnWndProc = WndProc;
		wndClass.style = CS_VREDRAW | CS_HREDRAW;
		wndClass.lpszClassName = WINDOWSWINDOW_CLASSNAME;

		if ( INVALID_ATOM == RegisterClassEx ( &wndClass ) )
			return E_FAIL;

		hWnd = CreateWindowEx ( 0, WINDOWSWINDOW_CLASSNAME, title, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr,
			wndClass.hInstance, nullptr );

		if ( hWnd == 0 )
			return E_FAIL;

		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE Show ()
	{
		if ( !ShowWindow ( hWnd, SW_SHOW ) )
			return E_FAIL;
		UpdateWindow ( hWnd );
		return S_OK;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE Run ( std::function<void ()> & run )
	{
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
				run ();
				Sleep ( 0 );
			}
		}
		if ( msg.wParam )
			return E_FAIL;
		return S_OK;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE GetClientSize ( UINT * width, UINT * height )
	{
		RECT rect;
		if ( !GetClientRect ( hWnd, &rect ) )
			return E_FAIL;
		*width = rect.right - rect.left;
		*height = rect.bottom - rect.top;
		return S_OK;
	}

public:
	virtual HRESULT STDMETHODCALLTYPE GetHandle ( HWND * handle )
	{
		*handle = hWnd;
		return S_OK;
	}

private:
	static LRESULT CALLBACK WndProc ( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
	{
		switch ( uMsg )
		{
			case WM_DESTROY:
				PostQuitMessage ( 0 );
				break;

			default:
				return DefWindowProc ( hWnd, uMsg, wParam, lParam );
		}

		return 0;
	}

private:
	HWND hWnd;
};

static HWND GetHandleFromWindow ( WindowsWindow * window )
{
	HWND handle;
	window->GetHandle ( &handle );
	return handle;
}