#include <std_include.hpp>
#include "loader/component_loader.hpp"
#include "resource.hpp"

#include "game/game.hpp"

#include <utils/thread.hpp>
#include <utils/hook.hpp>

#define CONSOLE_BUFFER_SIZE 16384
#define WINDOW_WIDTH 608

namespace console
{
	namespace
	{
		volatile bool g_started = false;
		HANDLE logo;

		void print_message(const char* message)
		{
			if (g_started)
			{
				game::Com_Printf(0, 0, "%s", message);
			}
		}

		void print_stub(const char* fmt, ...)
		{
			va_list ap;
			va_start(ap, fmt);

			char buffer[1024]{0};
			const int res = vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, fmt, ap);
			(void)res;
			print_message(buffer);

			va_end(ap);
		}

		LRESULT con_wnd_proc(const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam)
		{
			switch (msg)
			{
			case WM_CTLCOLOREDIT:
			case WM_CTLCOLORSTATIC:
				SetBkColor(reinterpret_cast<HDC>(wparam), RGB(50, 50, 50));
				SetTextColor(reinterpret_cast<HDC>(wparam), RGB(232, 230, 227));
				return reinterpret_cast<INT_PTR>(CreateSolidBrush(RGB(50, 50, 50)));
			case WM_CLOSE:
				game::Cbuf_AddText(0, "quit\n");
				[[fallthrough]];
			default:
				return utils::hook::invoke<LRESULT>(0x142333520_g, hwnd, msg, wparam, lparam);
			}
		}

		LRESULT input_line_wnd_proc(const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam)
		{
			return utils::hook::invoke<LRESULT>(0x142333820_g, hwnd, msg, wparam, lparam);
		}

		void sys_create_console_stub(const HINSTANCE h_instance)
		{
			// C6262
			char text[CONSOLE_BUFFER_SIZE];
			char clean_console_buffer[CONSOLE_BUFFER_SIZE];

			const auto* class_name = "BOIII WinConsole";
			const auto* window_name = "BOIII Console";

			WNDCLASSA wnd_class{};
			wnd_class.style = 0;
			wnd_class.lpfnWndProc = con_wnd_proc;
			wnd_class.cbClsExtra = 0;
			wnd_class.cbWndExtra = 0;
			wnd_class.hInstance = h_instance;
			wnd_class.hIcon = LoadIconA(h_instance, reinterpret_cast<LPCSTR>(1));
			wnd_class.hCursor = LoadCursorA(nullptr, reinterpret_cast<LPCSTR>(0x7F00));
			wnd_class.hbrBackground = CreateSolidBrush(RGB(50, 50, 50));
			wnd_class.lpszMenuName = nullptr;
			wnd_class.lpszClassName = class_name;

			if (!RegisterClassA(&wnd_class))
			{
				return;
			}

			RECT rect{};
			rect.left = 0;
			rect.right = 620;
			rect.top = 0;
			rect.bottom = 450;
			AdjustWindowRect(&rect, 0x80CA0000, 0);

			auto dc = GetDC(GetDesktopWindow());
			const auto swidth = GetDeviceCaps(dc, 8);
			const auto sheight = GetDeviceCaps(dc, 10);
			ReleaseDC(GetDesktopWindow(), dc);

			utils::hook::set<int>(game::s_wcd::windowWidth, (rect.right - rect.left + 1));
			utils::hook::set<int>(game::s_wcd::windowHeight, (rect.bottom - rect.top + 1));

			utils::hook::set<HWND>(game::s_wcd::hWnd, CreateWindowExA(
				                       0, class_name, window_name, 0x80CA0000, (swidth - 600) / 2, (sheight - 450) / 2,
				                       rect.right - rect.left + 1, rect.bottom - rect.top + 1, nullptr, nullptr,
				                       h_instance, nullptr));

			if (!*game::s_wcd::hWnd)
			{
				return;
			}

			// create fonts
			dc = GetDC(*game::s_wcd::hWnd);
			const auto n_height = MulDiv(8, GetDeviceCaps(dc, 90), 72);

			utils::hook::set<HFONT>(game::s_wcd::hfBufferFont, CreateFontA(
				                        -n_height, 0, 0, 0, 300, 0, 0, 0, 1u, 0, 0, 0, 0x31u, "Courier New"));

			ReleaseDC(*game::s_wcd::hWnd, dc);

			if (logo)
			{
				utils::hook::set<HWND>(game::s_wcd::codLogo, CreateWindowExA(
					                       0, "Static", nullptr, 0x5000000Eu, 5, 5, 0, 0, *game::s_wcd::hWnd,
					                       reinterpret_cast<HMENU>(1), h_instance, nullptr));
				SendMessageA(*game::s_wcd::codLogo, 0x172u, 0, reinterpret_cast<LPARAM>(logo));
			}

			// create the input line
			utils::hook::set<HWND>(game::s_wcd::hwndInputLine, CreateWindowExA(
				                       0, "edit", nullptr, 0x50800080u, 6, 400, WINDOW_WIDTH, 20, *game::s_wcd::hWnd,
				                       reinterpret_cast<HMENU>(0x65), h_instance, nullptr));
			utils::hook::set<HWND>(game::s_wcd::hwndBuffer, CreateWindowExA(
				                       0, "edit", nullptr, 0x50A00844u, 6, 70, WINDOW_WIDTH, 324, *game::s_wcd::hWnd,
				                       reinterpret_cast<HMENU>(0x64), h_instance, nullptr));
			SendMessageA(*game::s_wcd::hwndBuffer, WM_SETFONT, reinterpret_cast<WPARAM>(*game::s_wcd::hfBufferFont), 0);

			utils::hook::set<WNDPROC>(game::s_wcd::SysInputLineWndProc, reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
				                          *game::s_wcd::hwndInputLine, -4,
				                          reinterpret_cast<LONG_PTR>(input_line_wnd_proc))));
			SendMessageA(*game::s_wcd::hwndInputLine, WM_SETFONT, reinterpret_cast<WPARAM>(*game::s_wcd::hfBufferFont), 0);

			SetFocus(*game::s_wcd::hwndInputLine);
			game::Con_GetTextCopy(text, 0x4000);
			game::Conbuf_CleanText(text, clean_console_buffer);
			SetWindowTextA(*game::s_wcd::hwndBuffer, clean_console_buffer);
		}
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			const auto self = utils::nt::library::get_by_address(sys_create_console_stub);
			logo = LoadImageA(self.get_handle(), MAKEINTRESOURCEA(IMAGE_LOGO), 0, 0, 0, LR_COPYFROMRESOURCE);

			utils::hook::jump(printf, print_stub);

			this->terminate_runner_ = false;

			this->console_runner_ = utils::thread::create_named_thread("Console IO", [this]
			{
				{
					utils::hook::detour sys_create_console_hook;
					sys_create_console_hook.create(0x1423339C0_g, sys_create_console_stub);

					game::Sys_ShowConsole();
					g_started = true;
				}

				MSG msg{};
				while (!this->terminate_runner_)
				{
					if (PeekMessageW(&msg, nullptr, NULL, NULL, PM_REMOVE))
					{
						TranslateMessage(&msg);
						DispatchMessageW(&msg);
					}
					else
					{
						std::this_thread::sleep_for(1ms);
					}
				}
			});
		}

		void pre_destroy() override
		{
			this->terminate_runner_ = true;

			if (this->console_runner_.joinable())
			{
				this->console_runner_.join();
			}
		}

	private:
		std::atomic_bool terminate_runner_{false};
		std::thread console_runner_;
	};
}

REGISTER_COMPONENT(console::component)
