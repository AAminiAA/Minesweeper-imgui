#pragma once
#include "imgui.h"

inline ImVec2 operator+(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x + b.x, a.y + b.y); }
inline ImVec2 operator+=(ImVec2& lhs, const ImVec2& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; return lhs; }
inline ImVec2 operator-(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x - b.x, a.y - b.y); }
inline ImVec2 operator*(const ImVec2& a, float s) { return ImVec2(a.x * s, a.y * s); }
inline ImVec2 operator*=(ImVec2& lhs, float s) { lhs.x *= s; lhs.y *= s; return lhs; }
inline ImVec2 operator*(float s, const ImVec2& a) { return a * s; }
inline ImVec2 operator/(const ImVec2& a, float s) { return ImVec2(a.x / s, a.y / s); }

float clamp(float num, float low, float high) {
	float ans = num;
	if (num < low)
		ans = low;
	else if (num > high)
		ans = high;

	return ans;
}

/*
	It linearly interpolates between two positions a and b:

	t = 0.0f → result is a
	t = 1.0f → result is b
	t = 0.5f → result is halfway between a and b
*/
inline ImVec2 Lerp(const ImVec2& a, const ImVec2& b, float t) {
	return ImVec2(
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t
	);
}
