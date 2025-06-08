#pragma once
#include "imgui.h"

namespace imgui_wrapper {
	namespace styles {
		enum Styles_e {
			Spectrum_Light,
			Spectrum_Dark,
			Old,
			MS,
			Windark,

			Styles_Count
		};

		void StyleColorsSpectrum();
		void StyleOld();
		void StyleMS();
		void StyleWindark();
	}
}