#include "directv.h"

template <class T>
void SafeRelease(T*& p)
{
    if (p)
    {
        p->Release();
        p = nullptr;
    }
}



DirectVException::DirectVException(const char* message)
	: std::runtime_error(message),
	  hResult(S_OK) {}

DirectVException::DirectVException(const char* message, HRESULT hResult)
	: std::runtime_error(message),
	  hResult(hResult) {}

HRESULT DirectVException::getHresult() const noexcept
{
	return hResult;
}



SolidBrush::SolidBrush(ID2D1HwndRenderTarget* renderTarget, const D2D1_COLOR_F& colour)
{
	HRESULT hr = renderTarget->CreateSolidColorBrush(colour, &brush);
	if (!SUCCEEDED(hr)) throw DirectVException("Failed to create D2D brush.", hr);
}

SolidBrush::SolidBrush(SolidBrush&& o)
{
	brush = o.brush;
	o.brush = nullptr;
}

SolidBrush& SolidBrush::operator=(SolidBrush&& o)
{
	if (this != &o)
	{
		SafeRelease(brush);

		brush = o.brush;
		o.brush = nullptr;
	}
	return *this;
}

SolidBrush::~SolidBrush()
{
	SafeRelease(brush);
}



Bitmap::Bitmap(ID2D1HwndRenderTarget* renderTarget, IWICImagingFactory* wicFactory, const wchar_t* filename)
{
	HRESULT hr;

	IWICBitmapDecoder* decoder = nullptr;
	hr = wicFactory->CreateDecoderFromFilename(
		filename,
		NULL,
		GENERIC_READ,
		WICDecodeMetadataCacheOnDemand,
		&decoder
	);
	if (!SUCCEEDED(hr))
	{
		throw DirectVException("Failed to create WIC decoder.", hr);
	}

	IWICBitmapFrameDecode* frame = nullptr;
	hr = decoder->GetFrame(0, &frame);
	if (!SUCCEEDED(hr))
	{
		SafeRelease(decoder);
		throw DirectVException("Failed to get frame from image.", hr);
	}
	
	IWICFormatConverter* formatConverter = nullptr;
	hr = wicFactory->CreateFormatConverter(&formatConverter);
	if (!SUCCEEDED(hr))
	{
		SafeRelease(frame);
		SafeRelease(decoder);
		throw DirectVException("Failed to create WIC format converter.", hr);
	}

	hr = formatConverter->Initialize(
		frame,
		GUID_WICPixelFormat32bppPBGRA,
		WICBitmapDitherTypeNone,
		NULL,
		0.0f,
		WICBitmapPaletteTypeMedianCut
	);
	if (!SUCCEEDED(hr))
	{
		SafeRelease(formatConverter);
		SafeRelease(frame);
		SafeRelease(decoder);
		throw DirectVException("Failed to convert image format.", hr);
	}

	hr = renderTarget->CreateBitmapFromWicBitmap(formatConverter, NULL, &bitmap);
	if (!SUCCEEDED(hr))
	{
		SafeRelease(formatConverter);
		SafeRelease(frame);
		SafeRelease(decoder);
		throw DirectVException("Failed to create D2D bitmap from WIC bitmap.", hr);
	}
	
	SafeRelease(formatConverter);
	SafeRelease(frame);
	SafeRelease(decoder);

	size = bitmap->GetSize();
}

Bitmap::Bitmap(Bitmap&& o)
{
	bitmap = o.bitmap;
	size = o.size;
	o.bitmap = nullptr;
	o.size = {};
}

Bitmap& Bitmap::operator=(Bitmap&& o)
{
	if (this != &o)
	{
		SafeRelease(bitmap);

		bitmap = o.bitmap;
		size = o.size;
		o.bitmap = nullptr;
		o.size = {};
	}
	return *this;
}

Bitmap::~Bitmap()
{
	SafeRelease(bitmap);
}



Font::Font(IDWriteFactory* DWriteFactory, const wchar_t* fontFamily, float size, const wchar_t* locale)
{
	HRESULT hr = DWriteFactory->CreateTextFormat(
		fontFamily,
		NULL,
		DWRITE_FONT_WEIGHT_REGULAR,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		size,
		locale,
		&textFormat
	);
	if (!SUCCEEDED(hr)) throw DirectVException("Failed to create font.", hr);
    hr = textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	if (!SUCCEEDED(hr)) throw DirectVException("Failed to set text alignment.", hr);
    hr = textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	if (!SUCCEEDED(hr)) throw DirectVException("Failed to set paragraph alignment.", hr);
}

Font::Font(Font&& o) noexcept
{
	textFormat = o.textFormat;
	o.textFormat = nullptr;
}

Font& Font::operator=(Font&& o) noexcept
{
	if (this != &o)
	{
		SafeRelease(textFormat);

		textFormat = o.textFormat;
		o.textFormat = nullptr;
	}
	return *this;
}

Font::~Font()
{
	SafeRelease(textFormat);
}



void DirectV::initWindow(const wchar_t* title, int width, int height, WndType wndType)
{
	HRESULT hr;

    const wchar_t* windowClassName = L"DirectV window";
	WNDCLASSEXW wc = {};

    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = windowClassName;

	RegisterClassExW(&wc);

	DWORD windStyleEx = 0;
	DWORD windStyle;
	switch (wndType)
	{
		default:
		case WndType::NONRESIZEABLE:
		{
			windStyle = WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX;
		}
		break;
		case WndType::FULLSCREEN:
		{
			windStyle = WS_POPUP;
		}
		break;
		case WndType::RESIZEABLE:
		{
			windStyle = WS_OVERLAPPEDWINDOW;
		}
		break;
	}

	RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = width;
	rect.bottom = height;
	if (!AdjustWindowRectEx(&rect, windStyle, false, windStyleEx))
		throw DirectVException("Failed to adjust window rectangle.", HRESULT_FROM_WIN32(GetLastError()));
	this->width = rect.right - rect.left;
	this->height = rect.bottom - rect.top;

	HMENU hMenubar = CreateMenu();
	if (!hMenubar)
		throw DirectVException("Failed to create menu.", HRESULT_FROM_WIN32(GetLastError()));
	if (!AppendMenuW(hMenubar, MF_STRING, 101 /* <-- ID */, L"&Open"))
	{
		DestroyMenu(hMenubar);
		throw DirectVException("Failed to create menu.", HRESULT_FROM_WIN32(GetLastError()));
	}

	hWnd = CreateWindowExW(
		windStyleEx,
		windowClassName,
		title,
		windStyle,
		CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT, //rect.right-rect.left,
        CW_USEDEFAULT, //rect.bottom-rect.top,
		NULL,
		hMenubar,
		hInstance,
		this
	);
	if (hWnd == NULL)
	{
		DestroyMenu(hMenubar);
		throw DirectVException("Failed to create window.", HRESULT_FROM_WIN32(GetLastError()));
	}

	if (wndType == WndType::FULLSCREEN)
	{
		ShowWindow(hWnd, SW_MAXIMIZE); // Returnvärdet verkar inte ha något att göra med om funktionen lyckades eller ej.
	}
	else
	{
		ShowWindow(hWnd, SW_MAXIMIZE /* SW_NORMAL */);
	}
	if (!UpdateWindow(hWnd))
	{
		DestroyWindow(hWnd);
		throw DirectVException("Failed to update window.");
	}
}

void DirectV::initD2D()
{
	HRESULT hr;

	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &D2DFactory);
	if (!SUCCEEDED(hr))
	{
		DestroyWindow(hWnd);
		throw DirectVException("Failed to create D2D factory.", hr);
	}
	
	RECT rc;
	if (!GetClientRect(hWnd, &rc))
		throw DirectVException("Failed to get window size.", HRESULT_FROM_WIN32(GetLastError()));
	          
	renderTarget = NULL; 
	hr = D2DFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(
			hWnd,
			D2D1::SizeU(
				rc.right - rc.left,
				rc.bottom - rc.top)
		),
		&renderTarget
	);
	if (!SUCCEEDED(hr))
	{
		SafeRelease(D2DFactory);
		DestroyWindow(hWnd);
		throw DirectVException("Failed to create D2D render target.", hr);
	}

	wicFactory = nullptr;
	hr = CoCreateInstance(
		CLSID_WICImagingFactory,
		NULL,
		CLSCTX_INPROC_SERVER,
    	IID_PPV_ARGS(&wicFactory)
	);
	if (!SUCCEEDED(hr))
	{
		SafeRelease(renderTarget);
		SafeRelease(D2DFactory);
		DestroyWindow(hWnd);
		throw DirectVException("Failed to create WIC factory.", hr);
	}

	DWriteFactory = nullptr;
	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(&DWriteFactory)
	);
	if (!SUCCEEDED(hr))
	{
		SafeRelease(wicFactory);
		SafeRelease(renderTarget);
		SafeRelease(D2DFactory);
		DestroyWindow(hWnd);
		throw DirectVException("Failed to create DirectWrite factory.", hr);
	}

	D2DInitialised = true;
}

void DirectV::setD2DSize(int width, int height)
{
	HRESULT hr = renderTarget->Resize({static_cast<UINT32>(width), static_cast<UINT32>(height)});
	if (!SUCCEEDED(hr)) throw DirectVException("Failed to resize D2D render target.", hr);
}

DirectV::DirectV(HINSTANCE hInstance, const wchar_t* title, int width, int height, bool resizeable)
	: hInstance(hInstance),
	  keyData{},
	  lastChar(L'\0'),
	  lastKey('\0'),
	  D2DInitialised(false),
	  scale{1.0f, 1.0f, 0.0f, 0.0f},
	  rotationMatrix(D2D1::Matrix3x2F::Identity())
{
	initWindow(title, width, height, resizeable ? WndType::RESIZEABLE : WndType::NONRESIZEABLE);
	initD2D();
}

DirectV::DirectV(HINSTANCE hInstance, const wchar_t* title)
	: hInstance(hInstance),
	  keyData{},
	  lastChar(L'\0'),
	  lastKey('\0'),
	  D2DInitialised(false),
	  scale{1.0f, 1.0f, 0.0f, 0.0f},
	  rotationMatrix(D2D1::Matrix3x2F::Identity())
{
	initWindow(title, 0, 0, WndType::FULLSCREEN);
	initD2D();
}

DirectV::~DirectV()
{
	SafeRelease(DWriteFactory);
	SafeRelease(wicFactory);
	SafeRelease(renderTarget);
	SafeRelease(D2DFactory);
	if (IsWindow(hWnd)) DestroyWindow(hWnd);
}

void DirectV::resetTransform() noexcept
{
	scale = {1.0f, 1.0f, 0.0f, 0.0f};
	rotationMatrix = D2D1::Matrix3x2F::Identity();
	renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
}

void DirectV::rotateTransform(float degrees, float x, float y) noexcept
{
	rotationMatrix = D2D1::Matrix3x2F::Rotation(degrees, {x, y});
	renderTarget->SetTransform(rotationMatrix * D2D1::Matrix3x2F::Scale({scale.xFactor, scale.yFactor}, {scale.x, scale.y}));
}

void DirectV::scaleTransform(float xFactor, float yFactor, float x, float y) noexcept
{
	scale = {xFactor, yFactor, x, y};
	renderTarget->SetTransform(rotationMatrix * D2D1::Matrix3x2F::Scale({scale.xFactor, scale.yFactor}, {scale.x, scale.y}));
}

SolidBrush DirectV::createSolidBrush(const D2D1_COLOR_F& colour)
{
	return SolidBrush(renderTarget, colour);
}

Bitmap DirectV::createBitmap(const wchar_t* filename)
{
	return Bitmap(renderTarget, wicFactory, filename);
}

Font DirectV::createFont(const wchar_t* fontFamily, float size, const wchar_t* locale)
{
	return Font(DWriteFactory, fontFamily, size, locale);
}

void DirectV::beginDraw() noexcept
{
	renderTarget->BeginDraw();
}

void DirectV::endDraw()
{
	HRESULT hr = renderTarget->EndDraw();
	if (!SUCCEEDED(hr)) throw DirectVException("Drawing error.", hr);
}

void DirectV::clear() noexcept
{
	renderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
}

void DirectV::clear(const D2D1_COLOR_F& colour) noexcept
{
	renderTarget->Clear(colour);
}

void DirectV::drawRectangle(float x, float y, float width, float height, Brush& brush) noexcept
{
	renderTarget->DrawRectangle(D2D1::RectF(x, y, x + width, y + height), brush.getBrush());
}

void DirectV::fillRectangle(float x, float y, float width, float height, Brush& brush) noexcept
{
	renderTarget->FillRectangle(D2D1::RectF(x, y, x + width, y + height), brush.getBrush());
}

void DirectV::drawEllipse(float x, float y, float xRadius, float yRadius, Brush& brush) noexcept
{
	renderTarget->DrawEllipse({{x, y}, xRadius, yRadius}, brush.getBrush());
}

void DirectV::fillEllipse(float x, float y, float xRadius, float yRadius, Brush& brush) noexcept
{
	renderTarget->FillEllipse({{x, y}, xRadius, yRadius}, brush.getBrush());
}

void DirectV::drawBitmap(float x, float y, Bitmap& bitmap) noexcept
{
	renderTarget->DrawBitmap(bitmap.bitmap, {x, y, x + bitmap.getWidth(), y + bitmap.getHeight()}, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
}

void DirectV::drawBitmap(float x, float y, float width, float height, Bitmap& bitmap) noexcept
{
	renderTarget->DrawBitmap(bitmap.bitmap, {x, y, x + width, y + height}, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
}

void DirectV::drawBitmap(float x, float y, float sourceX, float sourceY, float sourceWidth, float sourceHeight, Bitmap& bitmap) noexcept
{
	renderTarget->DrawBitmap(bitmap.bitmap, {x, y, x + sourceWidth, y + sourceHeight}, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, {sourceX, sourceY, sourceX + sourceWidth, sourceY + sourceHeight});
}

void DirectV::drawBitmap(float x, float y, float width, float height, float sourceX, float sourceY, float sourceWidth, float sourceHeight, Bitmap& bitmap) noexcept
{
	renderTarget->DrawBitmap(bitmap.bitmap, {x, y, x + width, y + height}, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, {sourceX, sourceY, sourceX + sourceWidth, sourceY + sourceHeight});
}

void DirectV::drawText(float x, float y, float width, float height, const wchar_t* text, Font& font, Brush& brush) noexcept
{
	renderTarget->DrawTextW(
		text,
		wcslen(text),
		font.textFormat,
		{
			x,
			y,
			x + width,
			y + height
		},
		brush.getBrush()
	);
}

void DirectV::updateWindow()
{
	MSG msg;
    PeekMessageW(&msg, hWnd, 0, 0, PM_REMOVE);
	TranslateMessage(&msg);
	DispatchMessageW(&msg);
}

bool DirectV::keyDown(unsigned char key) noexcept
{
	return keyData[key];
}

void DirectV::clearChar() noexcept
{
	lastChar = L'\0';
}

wchar_t DirectV::getChar() noexcept
{
	wchar_t c = lastChar;
	lastChar = L'\0';
	return c;
}

void DirectV::clearKey() noexcept
{
	lastKey = '\0';
}

unsigned char DirectV::getKey() noexcept
{
	unsigned char k = lastKey;
	lastKey = '\0';
	return k;
}

LRESULT CALLBACK DirectV::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_CREATE)
	{
		// Sparar pekaren till DirectV:n i fönstret.
		LPCREATESTRUCTW cs = (LPCREATESTRUCTW)lParam;
		DirectV* directV = (DirectV*)cs->lpCreateParams;
		SetWindowLongPtrW(
			hWnd,
			GWLP_USERDATA,
			reinterpret_cast<LONG_PTR>(directV)
		);

		return 0;
	}

	// Hämtar pekaren till DirectV:n från fönstret.
	DirectV* directV = reinterpret_cast<DirectV*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
	if (directV)
	{
		switch (message)
		{
			case WM_CHAR:
			{
				directV->lastChar = wParam;
			}
			return 0;
			case WM_KEYDOWN:
			{
				directV->keyData[wParam] = true;
				directV->lastKey = static_cast<unsigned char>(wParam);
			}
			return 0;
			case WM_KEYUP:
			{
				directV->keyData[wParam] = false;
			}
			return 0;
			case WM_SIZE:
			{
				directV->width = LOWORD(lParam);
				directV->height = HIWORD(lParam);
				if (directV->D2DInitialised)
				{
					try
					{
						directV->setD2DSize(LOWORD(lParam), HIWORD(lParam));
					}
					catch (...) {} // Det går inte riktigt att göra något om setD2DSize() inte kan ändra storleken. (Men exceptionen borde nog fångas.)
				}
				else
				{
					return DefWindowProcW(hWnd, message, wParam, lParam);
				}
			}
			return 0;
		}
	}
	return DefWindowProcW(hWnd, message, wParam, lParam);
}



Timer::Timer(double framerate) noexcept
	: waitTime(1.0 / framerate),
	  delta(waitTime),
	  frameStartTime(TimerClock::now()) {}

void Timer::wait()
{
	const auto frameEndTime = TimerClock::now();
	const double d = (frameEndTime - frameStartTime).count() / 1000000000.0;

	std::this_thread::sleep_for(std::chrono::duration<double>(waitTime - d));
	const auto prevFrameStartTime = frameStartTime;
	frameStartTime = TimerClock::now();
	delta = (frameStartTime - prevFrameStartTime).count() / 1000000000.0;
}



void changeScreenResolution(int width, int height)
{
	DEVMODEW devMode {};
	devMode.dmSize = sizeof(devMode);
	devMode.dmPelsWidth = width;
	devMode.dmPelsHeight = height;
	devMode.dmFields = DM_PELSHEIGHT | DM_PELSWIDTH;
	LONG res = ChangeDisplaySettingsW(&devMode, CDS_FULLSCREEN);
	switch (res)
	{
		case DISP_CHANGE_BADDUALVIEW:
		{
			throw DirectVException("Could not change the screen resolution because the system is DualView capable.");
		}
		break;
		case DISP_CHANGE_BADFLAGS:
		{
			throw DirectVException("Could not change the screen resolution because the flags were invalid.");
		}
		break;
		case DISP_CHANGE_BADMODE:
		{
			throw DirectVException("Could not change the screen resolution because the screen resolution is not supported.");
		}
		break;
		case DISP_CHANGE_BADPARAM:
		{
			throw DirectVException("Could not change the screen resolution because the parameters were invalid.");
		}
		break;
		case DISP_CHANGE_FAILED:
		{
			throw DirectVException("Could not change the screen resolution because the display driver failed the graphics mode.");
		}
		break;
		case DISP_CHANGE_NOTUPDATED:
		{
			throw DirectVException("Could not change the screen resolution because the settings could not be written to the registry.");
		}
		break;
		case DISP_CHANGE_RESTART:
		{
			throw DirectVException("The computer must be restarted for the change to go into effect.");
		}
		break;
		default:
		break;
	}
}