#include <vector>
#include <memory>
#include <windowsx.h>
#include "directv.cpp"

#include <sstream>

struct ImageDispData
{
	std::unique_ptr<Bitmap> bitmap;
	float x;
	float y;
	float zoom;
};

/* Det finns bara en instans av DirectV så det går nog bra att använda en global variabel.
   För övrigt skulle det nog bli jobbigt att göra på något annat sätt då DirectV har sin
   pekare i fönstrets "window long ptr" (eller vad man nu ska kalla det). */
struct {
	bool mouseDown;
	float startX;
	float startY;
	float currX;
	float currY;
	short zoom;
} mouseData {false, 0.0f, 0.0f, 0.0f, 0.0f, 0};

std::unique_ptr<DirectV> dv;
std::unique_ptr<std::vector<ImageDispData>> imgs;
unsigned int currImg = 0;

LRESULT CALLBACK DafiWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	try
	{
		dv = std::make_unique<DirectV>(hInstance, L"Dafi", 500, 500, true);
		SetWindowLongPtrW(dv->getHwnd(), GWLP_WNDPROC, (LONG_PTR)DafiWndProc);
		DragAcceptFiles(dv->getHwnd(), TRUE);
		Timer t(60.0);

		imgs = std::make_unique<std::vector<ImageDispData>>();

		// Läs in argument.
		int argc;
		wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if (argv != NULL)
		{
			for (int i = 1; i < argc; i++)
			{
				try
				{
					imgs->push_back({std::make_unique<Bitmap>(dv->createBitmap(argv[i])), 0.0f, 0.0f, 1.0f});
				}
				catch (...)
				{
					MessageBoxW(
						dv->getHwnd(),
						(std::wstring(L"Unable to open image: ") + argv[i]).c_str(),
						L"Unable to open image.",
						MB_ICONWARNING
					);
				}
			}
			LocalFree(argv);
		}

		constexpr float speed = 400.0f; // För förflyttning med WASD.

		// Huvudloopen.
		while (dv->windowExists())
		{
			auto lastKey = dv->getKey();

			// Kod för förflyttning av bilden. Körs endast om en bild är vald.
			if (currImg < imgs->size())
			{
				// Zoomning.
				const auto zoomFactor = exp(mouseData.zoom / 480.0);
				mouseData.zoom = 0;
				(*imgs)[currImg].zoom *= zoomFactor;
				const auto mouseX = (mouseData.currX - (dv->getWidth() / 2.0f - (*imgs)[currImg].bitmap->getWidth() / 2.0f));
				const auto mouseY = (mouseData.currY - (dv->getHeight() / 2.0f - (*imgs)[currImg].bitmap->getHeight() / 2.0f));
				(*imgs)[currImg].x += (mouseX - (zoomFactor * mouseX)) / (*imgs)[currImg].zoom;
				(*imgs)[currImg].y += (mouseY - (zoomFactor * mouseY)) / (*imgs)[currImg].zoom;

				// Förflyttning med musen.
				if (mouseData.mouseDown)
				{
					(*imgs)[currImg].x += (mouseData.currX - mouseData.startX) / (*imgs)[currImg].zoom;
					mouseData.startX = mouseData.currX;
					(*imgs)[currImg].y += (mouseData.currY - mouseData.startY) / (*imgs)[currImg].zoom;
					mouseData.startY = mouseData.currY;
				}

				// Förflyttning med WASD.
				if (dv->keyDown('W'))
					(*imgs)[currImg].y += (speed / (*imgs)[currImg].zoom) * t.getDelta();
				if (dv->keyDown('A'))
					(*imgs)[currImg].x += (speed / (*imgs)[currImg].zoom) * t.getDelta();
				if (dv->keyDown('S'))
					(*imgs)[currImg].y -= (speed / (*imgs)[currImg].zoom) * t.getDelta();
				if (dv->keyDown('D'))
					(*imgs)[currImg].x -= (speed / (*imgs)[currImg].zoom) * t.getDelta();
			}

			// Det går endast att byta mellan bilder om det finns bilder.
			if (imgs->size() > 0)
			{
				if (lastKey == VK_LEFT)
					currImg = ((currImg + imgs->size()) - 1) % imgs->size();
				if (lastKey == VK_RIGHT)
					currImg = (currImg + 1) % imgs->size();
			}

			dv->beginDraw();
			dv->clear();

			if (currImg < imgs->size())
			{
				dv->scaleTransform((*imgs)[currImg].zoom, (*imgs)[currImg].zoom, 0.0f, 0.0f);
				dv->drawBitmap(
					(dv->getWidth() / 2.0f - (*imgs)[currImg].bitmap->getWidth() / 2.0f) / (*imgs)[currImg].zoom + (*imgs)[currImg].x,
					(dv->getHeight() / 2.0f - (*imgs)[currImg].bitmap->getHeight() / 2.0f) / (*imgs)[currImg].zoom + (*imgs)[currImg].y,
					*(*imgs)[currImg].bitmap
				);
			}

			/* dv->scaleTransform(1.0f, 1.0f, 0.0f, 0.0f); */ // <-- Behövs inte nu då inget annat ritas.

			dv->endDraw();
			dv->updateWindow();
			t.wait();
		}
	}
	catch (const std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "Fatal error", MB_ICONERROR);
		return EXIT_FAILURE;
	}
	catch (...)
	{
		MessageBoxW(nullptr, L"Unknown error", L"Fatal error", MB_ICONERROR);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

// Lägger till en bild i imgs. Visar ett felmeddelande om något går fel.
void openImage(const wchar_t* filename) noexcept
{
	try
	{
		imgs->push_back({
			std::make_unique<Bitmap>(
				dv->createBitmap(filename)
			),
			0.0f,
			0.0f,
			1.0f
		});
		currImg = imgs->size() - 1;
	}
	catch (...)
	{
		try
		{
			MessageBoxW(
				dv->getHwnd(),
				(std::wstring(L"Unable to open image: ") + filename).c_str(),
				L"Unable to open image.",
				MB_ICONWARNING
			);
		}
		catch (...) {}
	}
}

// Koden för att tolka datan som man får från "öppna"-dialogrutan är så lång att den fick bli sin egen funktion.
void processOpenDialogueResult(const OPENFILENAMEW& of, const wchar_t* buffer) noexcept
{
	/* Om användaren har valt flera filer finns sökvägen med en gång, i början. Därför måste
	   sökvägen sparas i en separat sträng. Storleken borde räcka. (I vissa fall är den nog
	   lite större än vad som behövs men det skadar nog inte.) */
	wchar_t* directory = new (std::nothrow) wchar_t[of.nFileOffset + 1];
	if (directory)
	{
		/* Om användaren har valt flera filer så är buffern en nullterminerad sekvens av nullterminerade strängar.
		   Då är den första strängen sökvägen som filerna finns i, och resten är filerna. Om användaren har valt en
		   fil så är buffern däremot bara en nullterminerad sträng med filens plats på datorn. Detta betyder att 
		   sökvägen följs av ett nulltecken om och endast om flera filer har valts, och detta används här för att
		   kolla om användaren har valt flera filer eller ej. Sökvägen kopieras in i en buffer samtidigt. */
		bool multiSelect = false;
		for (unsigned i = 0; true; i++)
		{
			if (i >= of.nFileOffset) // Själva filnamnet har nåtts först => endast en fil.
			{
				directory[i] = L'\0';
				break;
			}
			directory[i] = buffer[i];
			if (directory[i] == L'\0') // Ett nulltecken har nåtts först => flera filer.
			{
				multiSelect = true;
				break;
			}
		}

		if (multiSelect) // Flera filer
		{
			const wchar_t* currFilenameStart = &buffer[of.nFileOffset];
			while (true)
			{
				try
				{
					openImage((std::wstring(directory) + L"\\" + currFilenameStart).c_str());
				}
				catch (...)
				{
					MessageBoxW(dv->getHwnd(), L"Failed to open image.", L"Failed to open image.", MB_ICONWARNING);
				}
				while (*(currFilenameStart++) != L'\0');
				if (*currFilenameStart == L'\0') break;
			}
		}
		else // En fil
		{
			openImage(buffer);
		}

		delete[] directory;
	}
	else
	{
		MessageBoxW(dv->getHwnd(), L"Memory error.", L"Failed to open image.", MB_ICONWARNING);
	}
}

LRESULT CALLBACK DafiWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_LBUTTONDOWN:
		{
			SetCapture(hWnd);
			SetCursor((HCURSOR)LoadImageW(NULL, IDC_SIZEALL, IMAGE_CURSOR, 0, 0, LR_SHARED));
			mouseData.mouseDown = true;
			mouseData.startX = GET_X_LPARAM(lParam);
			mouseData.startY = GET_Y_LPARAM(lParam);
			mouseData.currX = mouseData.startX;
			mouseData.currY = mouseData.startY;
		}
		return 0;
		case WM_LBUTTONUP:
		{
			ReleaseCapture();
			mouseData.mouseDown = false;
		}
		return 0;
		case WM_SETCURSOR:
		{
			if (mouseData.mouseDown)
				SetCursor((HCURSOR)LoadImageW(NULL, IDC_SIZEALL, IMAGE_CURSOR, 0, 0, LR_SHARED));
			else
				return DefWindowProcW(hWnd, message, wParam, lParam);
		}
		return FALSE;
		case WM_MOUSEMOVE:
		{
			mouseData.currX = GET_X_LPARAM(lParam);
			mouseData.currY = GET_Y_LPARAM(lParam);
		}
		return 0;
		case WM_MOUSEWHEEL:
		{
			mouseData.zoom += GET_WHEEL_DELTA_WPARAM(wParam);
		}
		return 0;
		case WM_DROPFILES:
		{
			if (imgs)
			{
				const HDROP hDrop = (HDROP)wParam;
				const unsigned count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
				for (unsigned i = 0; i < count; i++)
				{
					const unsigned bufferSize = DragQueryFileW(hDrop, i, nullptr, 0) + 1;
					if (bufferSize)
					{
						wchar_t* buffer = new (std::nothrow) wchar_t[bufferSize];
						if (buffer)
						{
							if (DragQueryFileW(hDrop, i, buffer, bufferSize))
							{
								openImage(buffer);
							}
							delete[] buffer;
						}
					}
				}
				DragFinish(hDrop);
			}
		}
		return 0;
		case WM_MENUSELECT:
		{
			switch (LOWORD(wParam))
			{
				case 101: // Open
				{
					constexpr int bufferSize = 1000;
					wchar_t* buffer = new (std::nothrow) wchar_t[bufferSize];
					if (buffer)
					{
						buffer[0] = L'\0';
						OPENFILENAMEW of {
							sizeof(of),
							hWnd,
							nullptr,
							L"All Files\0*.*\0\0",
							nullptr,
							0,
							0,
							buffer,
							bufferSize,
							nullptr,
							0,
							nullptr,
							nullptr,
							OFN_ALLOWMULTISELECT | OFN_EXPLORER,
							OFN_FILEMUSTEXIST,
							0,
							0,
							0,
							0,
							nullptr,
							nullptr
						};
						if (GetOpenFileNameW(&of))
						{
							processOpenDialogueResult(of, buffer);
						}
						delete[] buffer;
					}
					else
					{
						MessageBoxW(dv->getHwnd(), L"Memory error.", L"Failed to open image.", MB_ICONWARNING);
					}
				}
				break;
			}
		}
		return 0;
	}
	return DirectV::WndProc(hWnd, message, wParam, lParam);
}