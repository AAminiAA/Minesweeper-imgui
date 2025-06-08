#pragma once

// Dear ImGui: standalone example application for DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>

#include <string>
#include <stdint.h>

#include "imgui_styles.h"

#define DEFAULT_CLEAR_COLOR		ImVec4(0.45f, 0.55f, 0.60f, 1.00f)

namespace imgui_wrapper {
	struct Application_properties {
		Application_properties(const uint32_t height, const uint32_t width, const wchar_t* title, const char* icon_path = nullptr)
			: height{ height }, width{ width }, window_title{ title }, icon{icon_path}, clear_color{ DEFAULT_CLEAR_COLOR } {}
		Application_properties() = delete;

		// Application level
		uint32_t height;
		uint32_t width;
		const wchar_t* window_title;
		const char* icon;
		ImVec4 clear_color;
	};

	void init(const Application_properties *const app_props);
	_NODISCARD bool start();
	void main_loop();
	void exit();

	void change_style(const uint8_t style);
	void load_font(const char* font_path, const float size);
	void load_multiple_size_font(const char* font_path, const float size);

	ID3D11Device* get_d3d11_Device();
	ID3D11DeviceContext* get_d3d11_DeviceContext();
	HWND get_HWND();

	// These 3 functions MUST be implemented by user

	// This function is called ONCE, right before the main loop
	// Program will be terminated if this function returned false
	bool USER_startup();

	// Define this function for your main application
	// This function is called inside the main loop
	void USER_main_app(bool& show_main_app, Application_properties *const app_props);

	// Clean-up function
	// This function is called after the program terminated
	void USER_cleanup();
}
