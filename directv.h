#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <wincodec.h>
#include <dwrite.h>

/*
 * Länka med:
 * - user32
 * - d2d1
 * - ole32
 * - dwrite
*/

using TimerClock = std::chrono::high_resolution_clock;

// Exceptionklass för alla exceptions som kommer från DirectV.
class DirectVException : public std::runtime_error
{
private:
	HRESULT hResult;
public:
	// Skapa en DirectVException utan en hResult. hResult sätts till S_OK.
	DirectVException(const char* message);
	// Skapa en DirectVException med en hResult.
	DirectVException(const char* message, HRESULT hResult);

	// Returnerar S_OK om det inte finns någon hResult.
	HRESULT getHresult() const noexcept;
};

class DirectV; // Forward-declarea för att SolidBrush ska kunna ha DirectV som en vän.

// Pure-virtual-klass som alla brushar deriverar från.
class Brush
{
protected:
	// Hämta brushen.
	virtual ID2D1Brush* getBrush() noexcept = 0;
public:
	virtual ~Brush() {}

	friend DirectV;
};

// Klass som representerar en enfärgad brush. Den kan inte skapas direkt, utan måste skapas av en DirectV.
class SolidBrush : public Brush
{
private:
	ID2D1SolidColorBrush* brush;

	// Skapa en SolidBrush. Det är tänkt att en DirectV ska göra detta.
	SolidBrush(ID2D1HwndRenderTarget* renderTarget, const D2D1_COLOR_F& colour);
protected:
	ID2D1Brush* getBrush() noexcept override {return brush;}
public:
	// Movekonstruktor.
	SolidBrush(SolidBrush&& o);
	// Moveoperator.
	SolidBrush& operator=(SolidBrush&& o);
	// Destruktor.
	~SolidBrush();

	// Ingen kopiering.
	SolidBrush(const SolidBrush&) = delete;
	// Ingen kopiering.
	SolidBrush& operator=(const SolidBrush&) = delete;
	
	friend DirectV;
};

// Klass som representerar en bitmap. Den kan inte skapas direkt, utan måste skapas av en DirectV.
class Bitmap
{
private:
	ID2D1Bitmap* bitmap;
	D2D1_SIZE_F size;

	// Skapa en Bitmap. Det är tänkt att en DirectV ska göra detta.
	Bitmap(ID2D1HwndRenderTarget* renderTarget, IWICImagingFactory* wicFactory, const wchar_t* filename);
public:
	// Movekonstruktor.
	Bitmap(Bitmap&& o);
	// Moveoperator
	Bitmap& operator=(Bitmap&& o);
	~Bitmap();

	// Ingen kopiering.
	Bitmap(const Bitmap&) = delete;
	// Ingen kopiering.
	Bitmap& operator=(const Bitmap&) = delete;

	// Returnerar bitmapens bredd.
	float getWidth() const noexcept {return size.width;}
	// Returnerar bitmapens höjd.
	float getHeight() const noexcept {return size.height;}

	friend DirectV;
};

// Klass som representerar en font. Den kan inte skapas direkt, utan måste skapas av en DirectV.
class Font
{
private:
	IDWriteTextFormat* textFormat;

	// Skapa en Font. Det är tänkt att en DirectV ska göra detta.
	Font(IDWriteFactory* DWriteFactory, const wchar_t* fontFamily, float size, const wchar_t* locale);
public:
	// Movekonstruktor.
	Font(Font&& o) noexcept;
	// Moveoperator.
	Font& operator=(Font&& o) noexcept;
	~Font();

	// Ingen kopiering.
	Font(const Font&) = delete;
	// Ingen kopiering.
	Font& operator=(const Font&) = delete;

	friend DirectV;
};

// Huvudklassen för DirectV. Den skapar fönster, initierar Direct2D etc. när man skapar den och tar bort det när den förstörs.
class DirectV
{
public:
	enum WndType
	{
		RESIZEABLE,
		NONRESIZEABLE,
		FULLSCREEN
	};
private:
	HINSTANCE hInstance;
	HWND hWnd;
	ID2D1Factory* D2DFactory;
	ID2D1HwndRenderTarget* renderTarget;
	IWICImagingFactory* wicFactory;
	bool keyData[0xff];
	unsigned char lastKey;
	wchar_t lastChar;
	bool D2DInitialised;
	IDWriteFactory* DWriteFactory;
	int width;
	int height;
	struct {
		float xFactor;
		float yFactor;
		float x;
		float y;
	} scale;
	D2D1_MATRIX_3X2_F rotationMatrix;

	// Körs när DirectV:n skapas.
	void initWindow(const wchar_t* title, int width, int height, WndType wndType);
	// Körs när DirectV:n skapas.
	void initD2D();

	void setD2DSize(int width, int height);
public:
	// Konstruktor som gör ett fönster med en bestämd storlek.
	DirectV(HINSTANCE hInstance, const wchar_t* title, int width, int height, bool resizeable = false);
	// Konstruktor som gör ett fullskärmsfönster.
	DirectV(HINSTANCE hInstance, const wchar_t* title);
	// Destruktor.
	~DirectV();

	// Ingen kopiering.
	DirectV(const DirectV&) = delete;
	// Ingen kopiering.
	DirectV& operator=(const DirectV&) = delete;

	// Återställer rotation o.d.
	void resetTransform() noexcept;
	// Roterar koordinatsystemet runt (x, y).
	void rotateTransform(float degrees, float x, float y) noexcept;
	// Skalar koordinatsystemet med (x, y) som centrum.
	void scaleTransform(float xFactor, float yFactor, float x, float y) noexcept;

	// Skapar en enfärgad brush.
	SolidBrush createSolidBrush(const D2D1_COLOR_F& colour);
	// Skapar en bitmap från en fil.
	Bitmap createBitmap(const wchar_t* filename);
	// Skapar en font.
	Font createFont(const wchar_t* fontFamily, float size, const wchar_t* locale = L"en-us");

	// Denna funktion ska köras innan man börjar rita.
	void beginDraw() noexcept;
	// Denna funktion ska köras när man har ritat klart. Kastar en exception om något har gått fel.
	void endDraw();

	// Rensar skärmen med svart färg.
	void clear() noexcept;
	// Rensar skärmen.
	void clear(const D2D1_COLOR_F& colour) noexcept;
	// Rita kanten av en rektangel.
	void drawRectangle(float x, float y, float width, float height, Brush& brush) noexcept;
	// Rita en fylld rektangel.
	void fillRectangle(float x, float y, float width, float height, Brush& brush) noexcept;
	// Rita kanten av en ellips.
	void drawEllipse(float x, float y, float xRadius, float yRadius, Brush& brush) noexcept;
	// Rita en fylld ellips.
	void fillEllipse(float x, float y, float xRadius, float yRadius, Brush& brush) noexcept;
	// Rita en bitmap.
	void drawBitmap(float x, float y, Bitmap& bitmap) noexcept;
	// Rita en bitmap med en annan storlek än den egentliga.
	void drawBitmap(float x, float y, float width, float height, Bitmap& bitmap) noexcept;
	// Rita en viss del av en bitmap.
	void drawBitmap(float x, float y, float sourceX, float sourceY, float sourceWidth, float sourceHeight, Bitmap& bitmap) noexcept;
	// Rita en viss del av en bitmap med en annan storlek än den egentliga.
	void drawBitmap(float x, float y, float width, float height, float sourceX, float sourceY, float sourceWidth, float sourceHeight, Bitmap& bitmap) noexcept;
	// Rita text.
	void drawText(float x, float y, float width, float height, const wchar_t* text, Font& font, Brush& brush) noexcept;

	// Hanterar meddelanden som fönstret har fått. Denna funktion bör köras varje frame.
	void updateWindow();

	// Returnerar true om tangenten är nedtryckt.
	bool keyDown(unsigned char key) noexcept;
	// Får DirectV:n att glömma det senaste skrivna tecknet.
	void clearChar() noexcept;
	// Returnerar det senaste skrivna tecknet, eller ett nulltecken om tecknet redan har hämtats med denna funktion.
	wchar_t getChar() noexcept;
	// Får DirectV:n att glömma den senaste nedtryckta tangenten.
	void clearKey() noexcept;
	// Returnerar den senaste nedtryckta tangenten, eller ett nulltecken om tangenten redan har hämtats med denna funktion.
	unsigned char getKey() noexcept;

	// Returnerar hWnd:et. Gör inte något dumt, som att ta bort fönstret.
	HWND getHwnd() noexcept {return hWnd;}
	// Returnerar renderTargeten.
	ID2D1HwndRenderTarget* getRenderTarget() noexcept {return renderTarget;}
	// Returnerar fönstrets (inre) bredd.
	int getWidth() const noexcept {return width;}
	// Returnerar fönstrets (inre) höjd.
	int getHeight() const noexcept {return height;}
	// Som getWidth() men den tar hänsyn till om man har använt scaleTransform().
	int getEffWidth() const noexcept {return width / scale.xFactor;}
	// Som getHeight() men den tar hänsyn till om man har använt scaleTransform().
	int getEffHeight() const noexcept {return height / scale.yFactor;}
	// Returnerar true om fönstret inte är stängt.
	bool windowExists() const noexcept {return IsWindow(hWnd);}

	// Wndproc:en som används till fönstren.
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

// En klass som kan användas för att försöka hålla en viss framerate.
class Timer
{
private:
	std::chrono::time_point<TimerClock> frameStartTime;
	double waitTime;
	double delta;
public:
	Timer(double framerate) noexcept;

	Timer(const Timer&) = delete;
	Timer& operator=(const Timer&) = delete;

	// Ändrar frameraten.
	void setFramerate(double framerate) noexcept {waitTime = 1.0 / framerate;}

	// Väntar tills nästa frame ska börja. Funktionen räknar bort tiden som har gått sedan förra gången wait() kördes.
	void wait();

	// Returnerar ett tal som man kan multiplicera med för att snabba upp saker när det laggar.
	double getDelta() const noexcept {return delta;}
	// Returnerar den riktiga frameraten.
	double getFramerate() const noexcept {return 1.0 / delta;}
};

// Ändrar skärmupplösningen. Skärmen återgår till den vanliga upplösningen när programmet avslutas.
void changeScreenResolution(int width, int height);