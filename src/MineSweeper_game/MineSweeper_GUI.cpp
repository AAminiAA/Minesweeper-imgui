#include "imgui_wrapper.h"
#include "MineSweeper.h"
#include "MS_Utilities.h"

#include <thread>
#include <map>
#include <functional>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

enum Fonts_e { Small, Medium, Larg, XLarg, XXLarg, Huge, Enurnos, FontsCount };
constexpr float g_FontSizes[FontsCount]{ 12.f, 18.f, 24.f, 32.f, 40.f, 50.f, 70.f };
ImFont* g_Fonts[FontsCount]{};

constexpr float g_MediumTextMediumSize_X{ 54.5831985f };

enum class GameSounds {
	Explosion, FlagCell, RevealCell, GameStart, GameWon, GameOver, Lucky, Unlucky, Count
};

constexpr const char* s_GameSoundsPaths[(int)GameSounds::Count]{
	"./resources/audio/explosion.wav", "./resources/audio/flag_cell.flac", "./resources/audio/reveal_cell.flac", "./resources/audio/game_start.wav",
	"./resources/audio/game_won.mp3", "./resources/audio/game_over.wav", "./resources/audio/lucky.wav", "./resources/audio/unlucky.mp3"
};

struct GamePages {
	bool Loading;
	bool Start;
	bool Main;
	bool Custom;
};
static GamePages s_Pages{};

struct GameStatus {
	// Be careful with this flag!
	bool IsInitialized = false;

	bool RestartGame = false;
	bool NewGame = false;
	bool LockInputs = false;
	bool SidebarVisible = true;

	bool FirstRun = true;
	bool FitGridToScreen = true;
	bool CloseCurrentGame = false;
	bool GameEnded = false;

	MiSw Pos LastMineExploded{};

	void restart() {
		RestartGame = false;
		LockInputs = false;
		FirstRun = true;
		GameEnded = false;
	}

	void newGame() {
		NewGame = false;
		LockInputs = false;
		FirstRun = true;
		GameEnded = false;
	}
};
static GameStatus s_Status;

static std::thread s_thr;
static MiSw PlayerStats s_Stats{};

struct MineSweeperImage {
	ID3D11ShaderResourceView* D3D11Texture = nullptr;
	ImTextureID ImGuiTexID = 0;
};

struct GameImages {
	MineSweeperImage Loading;
	MineSweeperImage Flag;
	MineSweeperImage Mine;
	MineSweeperImage Clock;
	MineSweeperImage EmojiHappy;
	MineSweeperImage EmojiSad;

	MineSweeperImage StartPageIcons[5]{};

	void release() {
		if (Flag.D3D11Texture) {
			Flag.D3D11Texture->Release();
			Flag.D3D11Texture = nullptr;
		}
		if (Mine.D3D11Texture) {
			Mine.D3D11Texture->Release();
			Mine.D3D11Texture = nullptr;
		}
		if (Clock.D3D11Texture) {
			Clock.D3D11Texture->Release();
			Clock.D3D11Texture = nullptr;
		}
		if (EmojiHappy.D3D11Texture) {
			EmojiHappy.D3D11Texture->Release();
			EmojiHappy.D3D11Texture = nullptr;
		}
		if (EmojiSad.D3D11Texture) {
			EmojiSad.D3D11Texture->Release();
			EmojiSad.D3D11Texture = nullptr;
		}
		for (int i{}; i < 5; i++) {
			if (StartPageIcons[i].D3D11Texture) {
				StartPageIcons[i].D3D11Texture->Release();
				StartPageIcons[i].D3D11Texture = nullptr;
			}
		}
	}
};
static GameImages s_Images;

static ma_engine s_SoundEngine;

struct AnimationState {
	bool active = false;
	double startTime = 0.0f;
};

struct GameAnimations {
	AnimationState GridReveal{};
	AnimationState GameWon{};
	AnimationState GameOver{};
	AnimationState RevealMines{};
};
static GameAnimations s_Animations;

static std::map<std::pair<int, int>, AnimationState> s_flipStatesMap;
static std::map<std::pair<int, int>, AnimationState> s_bounceMap;

enum class GamePopups {
	GameWon,
	GameOver,
	SecondChance,
	Restart,

	Count
};
static bool s_PopupStatus[(int)GamePopups::Count]{};

static void RestartPopupModal(minesweeper::MineSweeper& game);
static void SecondChancePopupModal(minesweeper::MineSweeper& game);
static void GameOverPopupModal(minesweeper::MineSweeper& game);
static void GameWonPopupModal(minesweeper::MineSweeper& game);

typedef void(*Fptr_PopupModalFunc)(minesweeper::MineSweeper& game);
static Fptr_PopupModalFunc s_PopupFuncs[(int)GamePopups::Count]{
	GameWonPopupModal, GameOverPopupModal, SecondChancePopupModal, RestartPopupModal
};


ImFont* GetClosestFont(const int cellSize) {
	for (int i{}; i < FontsCount; i++) {
		if (cellSize <= g_FontSizes[i])
			return g_Fonts[i];
	}

	return g_Fonts[Enurnos];
}

void GamePlaySound(const GameSounds sound) {
	if (sound < GameSounds(0) || sound >= GameSounds::Count)
		return;

	ma_engine_play_sound(&s_SoundEngine, s_GameSoundsPaths[(int)sound], NULL);
}

void DrawCellContent(const minesweeper::Cell& cell, const ImVec2 cellMin, const ImVec2 cellMax, const int cellSize, ImDrawList* drawList) {
	// Draw content
	if (cell.is_sweeped() && !cell.is_bomb() && cell.value > 0) {
		char buf[2];
		snprintf(buf, 2, "%d", cell.value);

		ImU32 numberColor;
		switch (cell.value) {
		case 1: numberColor = IM_COL32(0, 0, 255, 255); break;        // Blue
		case 2: numberColor = IM_COL32(0, 128, 0, 255); break;        // Green
		case 3: numberColor = IM_COL32(255, 0, 0, 255); break;        // Red
		case 4: numberColor = IM_COL32(0, 0, 128, 255); break;        // Navy
		case 5: numberColor = IM_COL32(128, 0, 0, 255); break;        // Maroon
		case 6: numberColor = IM_COL32(0, 128, 128, 255); break;      // Teal
		case 7: numberColor = IM_COL32(0, 0, 0, 255); break;          // Black
		case 8: numberColor = IM_COL32(128, 128, 128, 255); break;    // Gray
		default: numberColor = IM_COL32_BLACK; break;
		}

		ImFont* font = GetClosestFont(cellSize);
		float baseFontSize = font->FontSize; // Default size (e.g., 13.0f)
		float scaleFactor = (cellSize * 0.9f) / baseFontSize;
		float scaledFontSize = baseFontSize * scaleFactor;

		ImVec2 textSize = font->CalcTextSizeA(scaledFontSize, FLT_MAX, 0.0f, buf);
		ImVec2 textPos = {
			cellMin.x + (cellSize - textSize.x) * 0.5f,
			cellMin.y + (cellSize - textSize.y) * 0.5f
		};

		drawList->AddText(font, scaledFontSize, textPos, numberColor, buf);
	}
	// Cell is flagged
	else if (!cell.is_sweeped() && cell.is_marked()) {
		// rendering flag image
		const float size_scale_down = 0.15f * cellSize;
		ImVec2 ImageMin(cellMin.x + size_scale_down, cellMin.y + size_scale_down);
		ImVec2 ImageMax(cellMax.x - size_scale_down, cellMax.y - size_scale_down);

		drawList->AddImage(s_Images.Flag.ImGuiTexID, ImageMin, ImageMax);

	}
	else if (cell.is_sweeped() && cell.is_bomb()) {
		// rendering mine image
		const float size_scale_down = 0.08f * cellSize;
		ImVec2 ImageMin(cellMin.x + size_scale_down, cellMin.y + size_scale_down);
		ImVec2 ImageMax(cellMax.x - size_scale_down, cellMax.y - size_scale_down);

		drawList->AddImage(s_Images.Mine.ImGuiTexID, ImageMin, ImageMax);
	}
}

void DrawCellBackground(const minesweeper::Cell& cell, const minesweeper::Pos pos, const ImVec2 cellMin, const ImVec2 cellMax, const bool is_hovered, ImDrawList* drawList) {
	// Draw cell background
	ImU32 color = IM_COL32(200, 200, 200, 255); // default: hidden
	if (cell.is_sweeped()) {
		color = IM_COL32(150, 150, 150, 255); // revealed
		if (cell.is_bomb()) {
			color = IM_COL32(255, 100, 100, 255); // mine = red
		}
	}
	else if (cell.is_marked()) {
		color = IM_COL32(100, 150, 255, 255); // flag = blue
	}
	else if (is_hovered) {
		color = IM_COL32(170, 170, 170, 255); // hovered
	}

	float flipProgress = 1.0f;

	auto it = s_flipStatesMap.find({ pos.x, pos.y });
	if (it != s_flipStatesMap.end() && it->second.active) {
		double animTime = ImGui::GetTime() - it->second.startTime;
		const float duration = 0.15f;

		if (animTime < duration) {
			float t = animTime / duration;
			// NO FLIP
			//flipProgress = 1.0f - fabs(1.0f - 2.0f * t);  // triangle wave shape

			// Smoothstep ease-in-out (Hermite interpolation)
			//float ease = t * t * (3.0f - 2.0f * t);
			//flipProgress = ease;

			// Even smoother (Sine ease-in-out)
			float ease = 0.5f * (1.0f - cosf(t * 3.1415926f));
			flipProgress = ease;
		}
		else {
			it->second.active = false; // done animating
			flipProgress = 1.0f;
		}
	}

	ImVec2 center((cellMin.x + cellMax.x) * 0.5f, (cellMin.y + cellMax.y) * 0.5f);

	// We want to shrink a cell inward from both sides around its center
	// As progress goes from 1.0 → 0.0 → 1.0 (flip shape), the rectangle shrinks and grows — producing the flip effect.
	ImVec2 scaledMin = Lerp(center, cellMin, flipProgress);
	ImVec2 scaledMax = Lerp(center, cellMax, flipProgress);

	drawList->AddRectFilled(scaledMin, scaledMax, color, 4.0f);
	drawList->AddRect(scaledMin, scaledMax, IM_COL32_BLACK);


	//drawList->AddRectFilled(cellMin, cellMax, color, 0.0f);
	//drawList->AddRect(cellMin, cellMax, IM_COL32_BLACK);
}

void GridRevealAnimation(const minesweeper::Pos pos, float& computed_scale, float& computed_alpha) {
	const float animDuration = 0.3f;   // total animation per cell
	const float waveSpeed = 0.015f;    // delay between cells

	const float delay = (pos.x + pos.y) * waveSpeed;
	double time = ImGui::GetTime() - s_Animations.GridReveal.startTime;

	const float animProgress = (time - delay) / animDuration;

	computed_scale = 1.0f;
	computed_alpha = 1.0f;

	if (s_Animations.GridReveal.active) {
		if (animProgress < 0.0f) {
			computed_alpha = 0.0f;
			computed_scale = 0.8f;
		}
		else if (animProgress < 1.0f) {
			float ease = animProgress * animProgress * (3 - 2 * animProgress); // smoothstep
			computed_alpha = ease;
			computed_scale = 0.8f + 0.2f * ease;
		}
		else {
			computed_alpha = 1.0f;
			computed_scale = 1.0f;
		}

		// Stop animating when everything is done
		if (time > (9 + 9) * waveSpeed + animDuration)
			s_Animations.GridReveal.active = false;
	}
}

void CellBounceAnimation(const minesweeper::Pos pos, float& computed_scale) {
	computed_scale = 1.0f;

	auto it = s_bounceMap.find({ pos.x, pos.y });
	if (it != s_bounceMap.end() && it->second.active) {
		double t = ImGui::GetTime() - it->second.startTime;
		const float duration = 0.25f;
		if (t < duration) {
			float normalized = t / duration;
			computed_scale = 1.0f + 0.1f * sinf(normalized * 3.14159f); // bounce
		}
		else {
			it->second.active = false;
			computed_scale = 1.0f;
		}
	}
}

void CellFlipAnimation(const minesweeper::Pos pos) {
	float flipProgress = 1.0f;
	auto it = s_flipStatesMap.find({ pos.x, pos.y });

	if (it != s_flipStatesMap.end() && it->second.active) {
		double animTime = ImGui::GetTime() - it->second.startTime;
		const float duration = 0.15f;

		if (animTime < duration) {
			float t = animTime / duration;
			// NO FLIP
			//flipProgress = 1.0f - fabs(1.0f - 2.0f * t);  // triangle wave shape

			// Smoothstep ease-in-out (Hermite interpolation)
			//float ease = t * t * (3.0f - 2.0f * t);
			//flipProgress = ease;

			// Even smoother (Sine ease-in-out)
			float ease = 0.5f * (1.0f - cosf(t * 3.1415926f));
			flipProgress = ease;
		}
		else {
			it->second.active = false; // done animating
			flipProgress = 1.0f;
		}
	}
	// We want to shrink a cell inward from both sides around its center
	// As progress goes from 1.0 → 0.0 → 1.0 (flip shape), the rectangle shrinks and grows — producing the flip effect.
	//ImVec2 scaledMin = Lerp(center, cellMin, flipProgress);
	//ImVec2 scaledMax = Lerp(center, cellMax, flipProgress);
}

void DrawCellBackground2(const minesweeper::Cell& cell, const minesweeper::Pos pos, const ImVec2 cellMin, const ImVec2 cellMax, const bool is_hovered, ImDrawList* drawList)
{
	ImVec2 center = (cellMin + cellMax) * 0.5f;
	ImU32 borderColor = IM_COL32(0, 0, 0, 255);

	// 20% of the cell size
	float rounding = (cellMax.x - cellMin.x) * 0.2f;
	// 5% of the cell size
	float borderThickness = (cellMax.x - cellMin.x) * 0.05f;

	float cellAlpha = 1.f;

	//ImVec2 scaledMin = center + (cellMin - center) * cellScale * scale;
	//ImVec2 scaledMax = center + (cellMax - center) * cellScale * scale;

	ImVec2 scaledMin = cellMin - center;
	ImVec2 scaledMax = cellMax - center;

	if (s_Animations.GridReveal.active) {
		float reveal_scale{};
		GridRevealAnimation(pos, reveal_scale, cellAlpha);

		scaledMin *= reveal_scale;
		scaledMax *= reveal_scale;
	}

	float bounceScale{};
	CellBounceAnimation(pos, bounceScale);
	scaledMin *= bounceScale;
	scaledMax *= bounceScale;

	scaledMin += center;
	scaledMax += center;

	ImU32 bgColor{};
	if (cell.is_sweeped()) {
		// Reveal color
		// Apply alpha to cell background color
		bgColor = cell.is_bomb()
			? IM_COL32(255, 80, 80, static_cast<int>(255 * cellAlpha))  // red for mine
			: IM_COL32(220, 220, 220, static_cast<int>(255 * cellAlpha)); // light gray
	}
	else {
		// Hidden cell
		bgColor = is_hovered
			? IM_COL32(40, 110, 200, 200, static_cast<int>(255 * cellAlpha))
			: IM_COL32(60, 140, 220, 255, static_cast<int>(255 * cellAlpha));
	}

	drawList->AddRectFilled(scaledMin, scaledMax, bgColor, rounding);
	drawList->AddRect(scaledMin, scaledMax, borderColor, rounding, 0, borderThickness);
}

void HandleGridClick(minesweeper::MineSweeper& game, const minesweeper::Cell_Value row, const minesweeper::Cell_Value col, bool is_hovered) {

	if (ImGui::IsKeyPressed(ImGuiKey_F)) {
		// F key was pressed
		s_Status.FitGridToScreen = true;
	}

	if (s_Status.LockInputs)
		return;

	bool leftClick = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	bool rightClick = ImGui::IsMouseClicked(ImGuiMouseButton_Right);

	const auto& cell = game.get_cell(row, col);

	// Handle click
	if (is_hovered) {
		if (leftClick && !cell.is_sweeped() && !cell.is_marked()) {
			game.sweep(row, col);

			s_flipStatesMap[{row, col}] = { true, ImGui::GetTime() };
			s_bounceMap[{row, col}] = { true, ImGui::GetTime() };

			if (s_Status.FirstRun) {
				game.init_timer(ImGui::GetTime());
				game.start_timer();

				s_Status.FirstRun = false;
			}

			if (game.is_bomb(row, col)) {
				GamePlaySound(GameSounds::Explosion);
				s_Status.LastMineExploded.x = col;
				s_Status.LastMineExploded.y = row;
			}

			else
				GamePlaySound(GameSounds::RevealCell);

		}
		else if (rightClick && !cell.is_sweeped()) {
			game.toggle_mark(row, col);

			if (s_Status.FirstRun) {
				game.init_timer(ImGui::GetTime());
				game.start_timer();

				s_Status.FirstRun = false;
			}

			GamePlaySound(GameSounds::FlagCell);
		}
	}// is_hovered
}

void UpperRibbon(minesweeper::MineSweeper& game, bool& p_open) {
	if (ImGui::ArrowButton("BackToStartPage", ImGuiDir_Left)) {
		s_Status.CloseCurrentGame = true;
		return;
	}
	ImGui::SameLine();
	if (ImGui::Button("Restart")) {
		// Only show pop up when the game is not finished yet
		if (!s_Status.GameEnded) {
			ImGui::OpenPopup("Restart Game");
			s_PopupStatus[(int)GamePopups::Restart] = true;
		}	
		else
			s_Status.RestartGame = true;
	}

	ImGui::SameLine();
	ImGui::Text("Classic | %s %dx%d", game.get_diff_str(), game.height(), game.width());

	ImGui::SameLine();
	char button_label[13]{};
	sprintf_s(button_label, 13, "%s Sidebar", (s_Status.SidebarVisible) ? "Hide" : "Show");

	auto text_size = ImGui::CalcTextSize(button_label);
	auto viewport = ImGui::GetMainViewport();

	//auto posx = ImGui::GetContentRegionAvail().x;
	auto posx = viewport->WorkSize.x;
	posx -= text_size.x;
	posx -= 4 * ImGui::GetStyle().FramePadding.x;
	ImGui::SetCursorPosX(posx);
	if (ImGui::Button(button_label)) {
		s_Status.SidebarVisible = !s_Status.SidebarVisible;
		s_Status.FitGridToScreen = true;
	}
}

void InformationRibbon(minesweeper::MineSweeper& game) {

	ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(25, 25, 35, 240));

	if (ImGui::BeginChild("InfoRegion", ImVec2(0, 0), ImGuiChildFlags_Borders & 0, ImGuiWindowFlags_HorizontalScrollbar)) {
		// Vertical spacing
		ImGui::Dummy(ImVec2(0, 20));

		// === Emoji / Game State Icon ===
		auto avail_region = ImGui::GetContentRegionAvail();
		ImVec2 emojiSize(min(avail_region.x, avail_region.y) * 0.7f, min(avail_region.x, avail_region.y) * 0.7f);
		ImVec2 emojiPos = ImVec2((avail_region.x - emojiSize.x) * 0.5f, ImGui::GetCursorPosY());
		ImGui::SetCursorPos(emojiPos);
		if (game.is_game_over())
			ImGui::Image(s_Images.EmojiSad.ImGuiTexID, emojiSize);
		else
			ImGui::Image(s_Images.EmojiHappy.ImGuiTexID, emojiSize);

		ImGui::Dummy(ImVec2(0, 30));

		// === Mines Remaining ===
		ImGui::PushFont(g_Fonts[XLarg]);
		ImVec2 iconSize(g_FontSizes[XLarg], g_FontSizes[XLarg]);

		// Bomb icon
		ImVec2 bombIconPos = ImVec2((ImGui::GetContentRegionAvail().x - iconSize.x - 60) * 0.5f, ImGui::GetCursorPosY());
		ImGui::SetCursorPos(bombIconPos);
		ImGui::Image(s_Images.Mine.ImGuiTexID, iconSize);
		ImGui::SameLine();
		ImGui::Text("%d/%d", game.get_remaining_bombs(), game.get_mine_count());

		// === Timer ===
		ImVec2 clockIconPos = ImVec2((ImGui::GetContentRegionAvail().x - iconSize.x - 60) * 0.5f, ImGui::GetCursorPosY() + 10);
		ImGui::SetCursorPos(clockIconPos);
		ImGui::Image(s_Images.Clock.ImGuiTexID, iconSize);
		ImGui::SameLine();
		ImGui::Text("%d:%02d", (int)game.get_time() / 60, (int)game.get_time() % 60);

		ImGui::PopFont();
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();
}

int GetCellSize(const int width, const int height, const float padding) {
	const auto region_avail = ImGui::GetContentRegionAvail();
	//return (min(region_avail.x, region_avail.y) - width * padding) / width;
	auto cell_size = (region_avail.x - width * padding) / width;
	if ((cell_size * height + height * padding) > region_avail.y) {
		cell_size = (region_avail.y - height * padding) / height;
	}

	return cell_size;
}

void HandleGridZoom(float& zoom, ImVec2& panOffset) {
	//ImGui::Text("Zoom: %.1fx", zoom);
	if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl) {
		float scroll = ImGui::GetIO().MouseWheel;
		if (scroll != 0.0f) {
			float zoomCenterX = ImGui::GetIO().MousePos.x - panOffset.x;
			float zoomCenterY = ImGui::GetIO().MousePos.y - panOffset.y;

			float prevZoom = zoom;
			zoom *= (scroll > 0) ? 1.1f : 0.9f;
			zoom = clamp(zoom, 0.5f, 3.0f);

			// Adjust panOffset so zoom feels centered on cursor
			panOffset.x -= zoomCenterX * (zoom - prevZoom) / prevZoom;
			panOffset.y -= zoomCenterY * (zoom - prevZoom) / prevZoom;
		}
	}
}

void HandleGridPanning(ImVec2& panOffset) {
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
		ImVec2 delta = ImGui::GetIO().MouseDelta;
		panOffset.x += delta.x;
		panOffset.y += delta.y;
	}
}

void FitGridToScreen(const int width, const int height, const ImVec2& origin, ImVec2& origin_fit_offset, const float cell_size, float& cell_fit_facor, const float padding) {
	constexpr float empty_region_y = 0.2f;
	constexpr float empty_region_x = 0.1f;

	auto avail_region = ImGui::GetContentRegionAvail();
	auto grid_max_y = avail_region.y * (1.f - empty_region_y);

	auto fit_cell_size = (grid_max_y - height * padding) / height;

	auto cursor = ImGui::GetCursorScreenPos();

	ImVec2 fit_origin{};

	fit_origin.x = cursor.x + (avail_region.x - (fit_cell_size + padding) * width) / 2;
	fit_origin.y = cursor.y + avail_region.y * (empty_region_y / 2);

	// Means the cell size resulted in a grid that has a higher x dimention than available region
	if (fit_origin.x < 0) {
		auto grid_max_x = avail_region.x * (1.f - empty_region_x);
		fit_cell_size = (grid_max_x - width * padding) / width;

		fit_origin.x = cursor.x + avail_region.x * (empty_region_x / 2);
		fit_origin.y = cursor.y + (avail_region.y - (fit_cell_size + padding) * height) / 2;
	}

	origin_fit_offset.x = fit_origin.x - origin.x;
	origin_fit_offset.y = fit_origin.y - origin.y;

	cell_fit_facor = fit_cell_size / cell_size;
}

bool IsCellHovered(const ImVec2 cellMin, const ImVec2 cellMax) {
	ImVec2 mousePos = ImGui::GetIO().MousePos;

	// Outside of the window
	if (!ImGui::IsWindowHovered())
		return false;
	// Check for hover
	bool hovered = mousePos.x >= cellMin.x && mousePos.x <= cellMax.x &&
		mousePos.y >= cellMin.y && mousePos.y <= cellMax.y;

	return hovered;
}

void DrawMinesweeperGrid(minesweeper::MineSweeper& game) {

	static float zoom = 1.0f;
	static ImVec2 panOffset = ImVec2(0.0f, 0.0f);
	static ImVec2 originFitOffset = ImVec2(0.0f, 0.0f);
	static float cellFitFactor = 1.f;

	int width = game.width();
	int height = game.height();
	ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(15, 15, 25, 255)); // very dark blue-gray

	auto grid_region_size = ImVec2((s_Status.SidebarVisible) ? ImGui::GetContentRegionAvail().x * 0.8f : 0, 0);
	if (ImGui::BeginChild("GridRegion", grid_region_size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar)) {

		HandleGridZoom(zoom, panOffset);

		HandleGridPanning(panOffset);

		const float padding = 1.f;
		float cellSize{};
		ImVec2 origin{};

		cellSize = GetCellSize(width, height, padding) * zoom;
		origin = ImGui::GetCursorScreenPos();

		if (s_Status.FitGridToScreen) {
			s_Status.FitGridToScreen = false;

			panOffset.x = 0;
			panOffset.y = 0;

			zoom = 1.0f;
			cellSize = GetCellSize(width, height, padding);

			FitGridToScreen(width, height, origin, originFitOffset, cellSize, cellFitFactor, padding);
		}

		cellSize *= cellFitFactor;

		origin.x += panOffset.x + originFitOffset.x;
		origin.y += panOffset.y + originFitOffset.y;

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		for (int r = 0; r < height; ++r) {
			for (int c = 0; c < width; ++c) {
				const auto& cell = game.get_cell(r, c);

				ImVec2 cellMin = {
					origin.x + c * (cellSize + padding),
					origin.y + r * (cellSize + padding)
				};
				ImVec2 cellMax = {
					cellMin.x + cellSize,
					cellMin.y + cellSize
				};

				// Check for hover
				bool hovered = IsCellHovered(cellMin, cellMax);

				HandleGridClick(game, r, c, hovered);

				// Draw cell background
				DrawCellBackground2(cell, { r,c }, cellMin, cellMax, hovered, drawList);

				DrawCellContent(cell, cellMin, cellMax, cellSize, drawList);
			}
		}

		ImGui::Dummy(ImVec2((cellSize + padding) * width, (cellSize + padding) * height));
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void SetupPopupModalStyle() {
	// Center the modal window in the viewport when it appears
	ImVec2 windowSize = ImGui::GetMainViewport()->Size;
	ImVec2 windowPos = ImGui::GetMainViewport()->Pos;

	ImVec2 popupSize = windowSize * 0.5f;
	ImVec2 popupPos = ImVec2(windowPos.x + (windowSize.x - popupSize.x) * 0.5f,
		windowPos.y + (windowSize.y - popupSize.y) * 0.5f);

	//ImGui::SetNextWindowPos(popupPos, ImGuiCond_Appearing);
	//ImGui::SetNextWindowSize(popupSize, ImGuiCond_Appearing);
	ImGui::SetNextWindowPos(popupPos);
	ImGui::SetNextWindowSize(popupSize);

	// Push custom style variables for a more refined appearance
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);    // More rounded corners
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 20)); // Extra padding on all sides
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);        // Rounded buttons and frames

	// Optionally, modify title bar colors to further elevate the visual style
	ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.1f, 0.4f, 0.6f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.1f, 0.5f, 0.7f, 1.0f));
}

void PopModalStyle() {
	// Always pop the style variables in reverse order of pushing them
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(3);
}

void GameWonPopupModal(minesweeper::MineSweeper& game) {
	if (ImGui::BeginPopupModal("Game Won ;)", nullptr, ImGuiWindowFlags_NoScrollbar)) {

		ImGui::Text("Congratulations!");
		ImVec2 popupSize = ImGui::GetContentRegionAvail(); // Get available space inside the modal
		auto child_size = ImVec2(0, popupSize.y - ImGui::GetFont()->FontSize - 2 * ImGui::GetStyle().FramePadding.y);

		if (ImGui::BeginChild("Game Stats", child_size, ImGuiChildFlags_Borders)) {
			ImGui::Text("Game stats");
			if (game.get_difficulty() != minesweeper::Difficulty::Custom &&  s_Stats.BestTimes[(int)game.get_difficulty()] != minesweeper::PLAYER_TIME_NA)
				ImGui::Text("Best Time: %d:%02d", s_Stats.BestTimes[(int)game.get_difficulty()] / 60, s_Stats.BestTimes[(int)game.get_difficulty()] % 60);
			else
				ImGui::Text("Best Time: N/A");
			ImGui::Text("Time: %d:%02d", (int)game.get_time() / 60, (int)game.get_time() % 60);
			ImGui::Text("Mistakes: %d", game.get_exploded_mines());
		}
		ImGui::EndChild();
		//ImGui::Separator();
		if (ImGui::Button("New Game")) {
			s_Status.NewGame = true;

			s_PopupStatus[(int)GamePopups::GameWon] = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("View Board")) {
			s_Status.GameEnded = true;
			game.reveal_bombs();

			s_PopupStatus[(int)GamePopups::GameWon] = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Continue")) {
			s_Status.CloseCurrentGame = true;

			s_PopupStatus[(int)GamePopups::GameWon] = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void GameOverPopupModal(minesweeper::MineSweeper& game) {
	if (ImGui::BeginPopupModal("Game Over :(", nullptr, ImGuiWindowFlags_NoScrollbar)) {
		ImGui::Text("Better luck next time!");
		ImVec2 popupSize = ImGui::GetContentRegionAvail(); // Get available space inside the modal
		auto child_size = ImVec2(0, popupSize.y - ImGui::GetFont()->FontSize - 2 * ImGui::GetStyle().FramePadding.y);

		if (ImGui::BeginChild("Game Stats", child_size, ImGuiChildFlags_Borders)) {
			ImGui::Text("Game stats");
			if (s_Stats.BestTimes[(int)game.get_difficulty()] != minesweeper::PLAYER_TIME_NA)
				ImGui::Text("Best Time: %d:%02d", s_Stats.BestTimes[(int)game.get_difficulty()] / 60, s_Stats.BestTimes[(int)game.get_difficulty()] % 60);
			else
				ImGui::Text("Best Time: N/A");
			ImGui::Text("Time: %d:%02d", (int)game.get_time() / 60, (int)game.get_time() % 60);
		}
		ImGui::EndChild();

		if (ImGui::Button("New Game")) {
			s_Status.NewGame = true;

			s_PopupStatus[(int)GamePopups::GameOver] = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Try again")) {
			s_Status.RestartGame = true;

			s_PopupStatus[(int)GamePopups::GameOver] = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("View Board")) {
			s_Status.GameEnded = true;
			game.reveal_bombs();

			s_PopupStatus[(int)GamePopups::GameOver] = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Home")) {
			s_Status.GameEnded = true;
			s_Status.CloseCurrentGame = true;

			s_PopupStatus[(int)GamePopups::GameOver] = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void SecondChancePopupModal(minesweeper::MineSweeper& game) {
	if (ImGui::BeginPopupModal("Second Chance, Sorta!", nullptr, ImGuiWindowFlags_NoScrollbar)) {
		ImGui::Text("You steped on a mine!");
		ImVec2 popupSize = ImGui::GetContentRegionAvail(); // Get available space inside the modal
		auto child_size = ImVec2(0, popupSize.y - ImGui::GetFont()->FontSize - 2 * ImGui::GetStyle().FramePadding.y);

		if (ImGui::BeginChild("Second Chance", child_size, ImGuiChildFlags_Borders)) {
			ImGui::Text("Feeling lucky today?");
		}
		ImGui::EndChild();
		//ImGui::Separator();
		static bool show_game_over_modal = false;
		if (ImGui::Button("Cheat!")) {
			game.revive_game();
			game.start_timer();

			s_Status.LockInputs = false;
			s_Status.GameEnded = false;

			s_PopupStatus[(int)GamePopups::SecondChance] = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Roll a dice!")) {
			if (rand() % 2) {
				game.revive_game();
				game.start_timer();

				s_Status.LockInputs = false;
				s_Status.GameEnded = false;
				GamePlaySound(GameSounds::Lucky);
			}
			else {
				s_Animations.RevealMines.active = true;
				s_Animations.RevealMines.startTime = ImGui::GetTime();
				GamePlaySound(GameSounds::Unlucky);
			}

			s_PopupStatus[(int)GamePopups::SecondChance] = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();

		if (ImGui::Button("Concede")) {
			s_Status.FitGridToScreen = true;

			s_Animations.RevealMines.active = true;
			s_Animations.RevealMines.startTime = ImGui::GetTime();

			s_PopupStatus[(int)GamePopups::SecondChance] = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void RestartPopupModal(minesweeper::MineSweeper& game) {
	if (ImGui::BeginPopupModal("Restart Game", nullptr, ImGuiWindowFlags_NoScrollbar)) {
		static bool show_message{};
		ImVec2 popupSize = ImGui::GetContentRegionAvail(); // Get available space inside the modal
		auto child_size = ImVec2(0, popupSize.y - ImGui::GetFont()->FontSize - 2 * ImGui::GetStyle().FramePadding.y);

		if (ImGui::BeginChild("Second Chance", child_size)) {
			ImGui::Text("Are you sure you want to restart this game?");
			ImGui::Text("All progress in this game will be lost!");
			ImGui::Dummy(ImVec2(0, 10));
			ImGui::Separator();
			ImGui::Checkbox("Don't show this message again", &show_message);
		}
		ImGui::EndChild();

		if (ImGui::Button("Cancel")) {
			s_PopupStatus[(int)GamePopups::Restart] = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Restart")) {
			s_Status.RestartGame = true;

			s_PopupStatus[(int)GamePopups::Restart] = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void ShowPopups(minesweeper::MineSweeper& game) {
	for (int i = 0; i < (int)GamePopups::Count; i++) {
		if (s_PopupStatus[i]) {
			SetupPopupModalStyle();

			s_PopupFuncs[i](game);

			PopModalStyle();
			break;
		}
	}
}

void RevealMinesInStyle(minesweeper::MineSweeper& game, sc::milliseconds total_anim_time) {
	auto center_row = s_Status.LastMineExploded.y;
	auto center_col = s_Status.LastMineExploded.x;

	int wave_index = 1;
	const auto max_index = max(game.height(), game.width());
	while (wave_index < max_index) {

		for (int i{}; i < 2 * wave_index + 1; i++) {

			if (center_row - wave_index + i < 0 || center_row - wave_index + i >= game.height())
				continue;

			for (int j{}; j < 2 * wave_index + 1; j++) {

				if (i != 0 && i != 2 * wave_index) {
					if (j > 0) {
						j = 2 * wave_index;
					}
				}

				if (center_col - wave_index + j < 0 || center_col - wave_index + j >= game.width())
					continue;

				auto& cell = game.get_cell(center_row - wave_index + i, center_col - wave_index + j);
				if (!cell.is_marked() && cell.is_bomb()) {
					cell.state = minesweeper::Cell_State::Sweeped;
					GamePlaySound(GameSounds::Explosion);
				}

			}
		}

		wave_index++;

		std::this_thread::sleep_for(total_anim_time / (max_index + 1));
	}
}

void HandleGameEnd(minesweeper::MineSweeper& game) {
	static constexpr double game_won_anim_duration = 3.f;
	if (s_Animations.GameWon.active) {
		float time = ImGui::GetTime() - s_Animations.GameWon.startTime;

		if (time > game_won_anim_duration) {
			s_Animations.GameWon.active = false;
			ImGui::OpenPopup("Game Won ;)");
			s_PopupStatus[(int)GamePopups::GameWon] = true;
		}

	}
	else if (s_Animations.GameOver.active) {
		static constexpr double game_over_anim_duration = 1.f;
		double time = ImGui::GetTime() - s_Animations.GameOver.startTime;


		if (time > game_over_anim_duration) {
			s_Animations.GameOver.active = false;
			ImGui::OpenPopup("Second Chance, Sorta!");
			s_PopupStatus[(int)GamePopups::SecondChance] = true;
		}
	}
	else if (s_Animations.RevealMines.active) {
		static constexpr double reveal_mines_anim_duration = 2.f;
		static bool animation_running = false;

		double time = ImGui::GetTime() - s_Animations.RevealMines.startTime;

		if (!animation_running) {
			animation_running = true;

			if (s_thr.joinable()) {
				s_thr.join(); // Ensure the previous thread is joined
			}
			s_thr = std::thread(RevealMinesInStyle, std::ref(game), sc::milliseconds((size_t)reveal_mines_anim_duration * 1000));
		}


		if (time > reveal_mines_anim_duration) {
			s_Animations.RevealMines.active = false;
			animation_running = false;
			ImGui::OpenPopup("Game Over :(");
			s_PopupStatus[(int)GamePopups::GameOver] = true;
		}
	}
}

void HandleGameState(minesweeper::MineSweeper& game, bool& p_open) {
	if (s_Status.RestartGame) {
		game.restart_game();
		s_Status.restart();

		return;
	}
	else if (s_Status.NewGame) {
		game.new_game();
		s_Status.newGame();

		return;
	}
	else if (s_Status.CloseCurrentGame) {
		s_Status.GameEnded = false;
		s_Status.CloseCurrentGame = false;
		p_open = false;
		s_Pages.Start = true;
		return;
	}

	// Using g_LockInputs to prevent calling this function every frame
	if (!s_Status.GameEnded && (game.is_game_over())) {
		s_Status.GameEnded = true;
		s_Status.LockInputs = true;

		game.stop_timer();
		GamePlaySound(GameSounds::GameOver);

		s_Animations.GameOver.active = true;
		s_Animations.GameOver.startTime = ImGui::GetTime();

	}
	else if (!s_Status.GameEnded && game.is_game_won()) {
		s_Status.GameEnded = true;
		s_Status.LockInputs = true;

		game.stop_timer();
		// If this run was the best player has done
		if (game.get_difficulty() != minesweeper::Difficulty::Custom) {
			if (s_Stats.BestTimes[(int)game.get_difficulty()] == minesweeper::PLAYER_TIME_NA) {
				s_Stats.BestTimes[(int)game.get_difficulty()] = game.get_time();
			}
			else if (game.get_time() < s_Stats.BestTimes[(int)game.get_difficulty()]) {
				s_Stats.BestTimes[(int)game.get_difficulty()] = game.get_time();
			}
		}

		GamePlaySound(GameSounds::GameWon);
		s_Animations.GameWon.active = true;
		s_Animations.GameWon.startTime = ImGui::GetTime();
	}

	if (s_Status.GameEnded) {
		HandleGameEnd(game);
	}

	game.update_timer(ImGui::GetTime());
	ShowPopups(game);
}

void MainGamePage(minesweeper::MineSweeper& game, bool& p_open) {
	static ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
		/*| ImGuiWindowFlags_MenuBar*/;

	// Making window full screen
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	if (ImGui::Begin("Minesweeper_MainPage", nullptr, window_flags)) {
		UpperRibbon(game, p_open);

		DrawMinesweeperGrid(game);

		ImGui::SameLine();
		if (s_Status.SidebarVisible)
			InformationRibbon(game);

		HandleGameState(game, p_open);
	}
	ImGui::End();
}

ImFont* GetOptionTextFont(const int option_size) {
	auto text_size = g_MediumTextMediumSize_X;
	auto chosen_font = g_Fonts[Medium];
	auto max_button_occupation = option_size * 0.65f;

	int i{ Medium + 1 };
	while (i < FontsCount) {
		text_size *= g_FontSizes[i] / g_FontSizes[i - 1];

		if (text_size > max_button_occupation) {
			return chosen_font;
		}

		chosen_font = g_Fonts[i];
		i++;
	}

	return chosen_font;
}

void DrawOptionContent(const int index, const ImVec2& cellMin, const ImVec2& cellMax, const int option_size, ImDrawList* drawList) {
	static constexpr const char* Options[]{ "Easy", "Medium", "Hard", "Expert", "Custom" };

	auto font = GetOptionTextFont(option_size);

	ImVec2 name_textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, Options[index]);
	ImVec2 name_textPos = {
		cellMin.x + (option_size - name_textSize.x) * 0.5f,
		cellMin.y + (option_size - name_textSize.y) * 0.5f
	};
	drawList->AddText(font, font->FontSize, name_textPos, IM_COL32_BLACK, Options[index]);

	char buf[8]{};
	if (index < (int)minesweeper::Difficulty::Count) {
		sprintf_s(buf, 8, "%d x %d", minesweeper::MineSweeper::s_Preset_Grid_sizes[index].y, minesweeper::MineSweeper::s_Preset_Grid_sizes[index].x);
	}
	else {
		strcpy_s(buf, 8, "? x ?");
	}

	auto dims_font_size = font->FontSize * 0.70f;
	ImVec2 dims_textSize = font->CalcTextSizeA(dims_font_size, FLT_MAX, 0, buf);
	ImVec2 dims_textPos = {
		cellMin.x + (option_size - dims_textSize.x) * 0.5f,
		cellMax.y - dims_textSize.y - dims_textSize.y * 0.5f
	};

	// Dims are 30% smaller than option text
	drawList->AddText(font, dims_font_size, dims_textPos, IM_COL32_BLACK, buf);
}

void StartPage(minesweeper::MineSweeper& game, bool& p_open) {

	static ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
		/*| ImGuiWindowFlags_MenuBar*/;

	// Making window full screen
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	//ImVec4 bgColor = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);  // Dark gray
	//ImVec4 bgColor = ImVec4(0.2f, 0.22f, 0.27f, 1.0f);  // Deep slate blue
	//ImVec4 bgColor = ImVec4(0.20f, 0.22f, 0.27f, 1.0f);  // Deep slate blue
	//ImVec4 bgColor = ImVec4(0.92f, 0.18f, 0.29f, 1.0f);  // Deep slate blue
	ImVec4 bgColor = ImVec4(0.05f, 0.05f, 0.15f, 1.0f);  // Deep blue-black


	//ImVec4 accentColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);  // Orange glow
	ImGui::PushStyleColor(ImGuiCol_WindowBg, bgColor); // very dark blue-gray
	if (ImGui::Begin("StartPage", nullptr, window_flags)) {
		const auto region_avail = ImGui::GetContentRegionAvail();

		auto option_size = (region_avail.x) / 5;
		//auto option_size = (region_avail.x - 5 * option_padding) / 5;
		auto option_padding = option_size * 0.05f;
		option_size -= option_padding;

		ImVec2 mousePos = ImGui::GetIO().MousePos;
		ImVec2 origin = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		for (int i{}; i < 5; i++) {
			//ImU32 color = IM_COL32(150, 150, 150, 0); // revealed

			ImVec2 cellMin = {
				origin.x + i * (option_size + option_padding),
				origin.y + (option_size + option_padding)
			};
			ImVec2 cellMax = {
				cellMin.x + option_size,
				cellMin.y + option_size
			};

			bool hovered = mousePos.x >= cellMin.x && mousePos.x <= cellMax.x &&
				mousePos.y >= cellMin.y && mousePos.y <= cellMax.y;

			if (hovered) {
				ImU32 color = IM_COL32(150, 150, 150, 100); // revealed
				drawList->AddRectFilled(cellMin, cellMax, color, 0.0f);
				drawList->AddRect(cellMin, cellMax, IM_COL32_WHITE, 0, 0, option_size * 0.03f);
			}

			drawList->AddImage(s_Images.StartPageIcons[i].ImGuiTexID, cellMin, cellMax);


			//DrawOptionContent(i, cellMin, cellMax, option_size, drawList);

			bool leftClick = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
			bool rightClick = ImGui::IsMouseClicked(ImGuiMouseButton_Right);

			// Handle click
			if (hovered && leftClick) {
				s_Status.newGame();

				s_Status.FitGridToScreen = true;

				if (i < (int)minesweeper::Difficulty::Count) {
					game.new_game(static_cast<minesweeper::Difficulty>(i));
					GamePlaySound(GameSounds::GameStart);

					p_open = false;
					s_Pages.Main = true;

					s_Animations.GridReveal.active = true;
					s_Animations.GridReveal.startTime = ImGui::GetTime();
				}
				// Custom game
				else {
					p_open = false;
					s_Status.LockInputs = true;

					game.new_game(20, 30, 144);
					game.randomly_flag_mine_count();
					s_Pages.Custom = true;
				}
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleColor();
}

void CustomGameConfigPage(minesweeper::MineSweeper& game, bool &p_open) {
	constexpr float MAX_MINE_RATIO{ 0.5f };
	constexpr float MIN_MINE_RATIO{ 0.15f };

	static int height{ game.height() };
	static int width{ game.width() };
	static int mines{ game.get_mine_count() };

	static ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
		/*| ImGuiWindowFlags_MenuBar*/;

	// Making window full screen
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	if (ImGui::Begin("Minesweeper_CustomPage", nullptr, window_flags)) {
		if (game.height() != height || game.width() != width) {
			game.new_game(height, width, mines);
			game.randomly_flag_mine_count();
		}

		if (ImGui::ArrowButton("BackToStartPage", ImGuiDir_Left)) {
			p_open = false;
			s_Pages.Start = true;
			ImGui::End();
			return;
		}
		ImGui::SameLine();

		ImGui::SameLine();
		ImGui::Text("Classic | %s %dx%d", game.get_diff_str(), game.height(), game.width());

		DrawMinesweeperGrid(game);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(25, 25, 35, 240));

		if (ImGui::BeginChild("InfoRegion", ImVec2(0, 0), ImGuiChildFlags_Borders & 0, ImGuiWindowFlags_HorizontalScrollbar)) {
			// Vertical spacing
			ImGui::Dummy(ImVec2(0, 20));

			// === Emoji / Game State Icon ===
			auto avail_region = ImGui::GetContentRegionAvail();
			ImVec2 emojiSize(min(avail_region.x, avail_region.y) * 0.7f, min(avail_region.x, avail_region.y) * 0.7f);
			ImVec2 emojiPos = ImVec2((avail_region.x - emojiSize.x) * 0.5f, ImGui::GetCursorPosY());
			ImGui::SetCursorPos(emojiPos);
			if (game.is_game_over())
				ImGui::Image(s_Images.EmojiSad.ImGuiTexID, emojiSize);
			else
				ImGui::Image(s_Images.EmojiHappy.ImGuiTexID, emojiSize);

			ImGui::Dummy(ImVec2(0, 30));

			//ImGui::PushFont(g_Fonts[XLarg]);

			// === Height ===
			if (ImGui::SliderInt("Height", &height, 9, 40)) {
				game.new_game(height, width, mines);
				game.randomly_flag_mine_count();

				if (mines > (height * width) * MAX_MINE_RATIO)
					mines = (height * width) * MAX_MINE_RATIO;
				else if (mines < (height * width) * MIN_MINE_RATIO) {
					mines = (height * width) * MIN_MINE_RATIO;
				}
			}

			// === Width ===
			if (ImGui::SliderInt("Width", &width, 9, 40)) {
				game.new_game(height, width, mines);
				game.randomly_flag_mine_count();

				if (mines > (height * width) * MAX_MINE_RATIO)
					mines = (height * width) * MAX_MINE_RATIO;
				else if (mines < (height * width) * MIN_MINE_RATIO) {
					mines = (height * width) * MIN_MINE_RATIO;
				}
			}

			// === Bombs ===
			if (ImGui::SliderInt("Mines", &mines, (height * width) * 0.15f, (height * width) * MAX_MINE_RATIO)) {
				game.new_game(height, width, mines);
				game.randomly_flag_mine_count();
			}

			ImGui::Text("Ratio: %.2f%%", (float)mines/(height * width) * 100);
			//ImGui::PopFont();
		}
		ImGui::Dummy(ImVec2(0, 30));

		if (ImGui::Button("Play", ImVec2(ImGui::GetContentRegionAvail().x, 0.f))) {
			game.clear_flags();
			p_open = false;
			s_Pages.Main = true;
			s_Status.newGame();

			GamePlaySound(GameSounds::GameStart);
		}

		ImGui::EndChild();
		ImGui::PopStyleColor();

	}
	ImGui::End();
}

bool HasWindowSizeChanged() {
	static int lastW = -1, lastH = -1;

	RECT rect;
	HWND hwnd = imgui_wrapper::get_HWND();
	GetClientRect(hwnd, &rect);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;

	if (w != lastW || h != lastH) {
		lastW = w;
		lastH = h;
		return true;
	}
	return false;
}

void LoadingPage() {
	static ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

	// Making window full screen
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	if (ImGui::Begin("LoadingPage", nullptr, window_flags)) {
		ImGui::Image(s_Images.Loading.ImGuiTexID, ImGui::GetContentRegionAvail());
	}
	ImGui::End();

	if (s_Status.IsInitialized) {
		if (s_thr.joinable()) {
			s_thr.join();
		}
		s_Images.Loading.D3D11Texture->Release();

		s_Pages.Loading = false;
		s_Pages.Start = true;
	}

}

void imgui_wrapper::USER_main_app(bool& show_main_app, Application_properties* const app_props) {
	static minesweeper::MineSweeper game(minesweeper::Difficulty::Easy);


	if (s_Pages.Loading) {
		LoadingPage();
	}
	else if (s_Pages.Start) {
		StartPage(game, s_Pages.Start);
	}
	else if (s_Pages.Main) {
		MainGamePage(game, s_Pages.Main);
	}
	else if (s_Pages.Custom) {
		CustomGameConfigPage(game, s_Pages.Main);
	}

	if (HasWindowSizeChanged())
		s_Status.FitGridToScreen = true;
}

ID3D11ShaderResourceView* LoadTextureFromFile(
	const char* filename,
	ID3D11Device* device,
	ID3D11DeviceContext* context
)
{
	int width, height, channels;
	unsigned char* imageData = stbi_load(filename, &width, &height, &channels, 4); // force RGBA

	if (!imageData)
		return nullptr;

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = imageData;
	initData.SysMemPitch = width * 4;

	ID3D11Texture2D* tex = nullptr;
	HRESULT hr = device->CreateTexture2D(&desc, &initData, &tex);

	stbi_image_free(imageData);
	if (FAILED(hr))
		return nullptr;

	ID3D11ShaderResourceView* outSRV = nullptr;
	hr = device->CreateShaderResourceView(tex, nullptr, &outSRV);
	tex->Release();

	return SUCCEEDED(hr) ? outSRV : nullptr;
}

bool load_flag_icon() {
	constexpr const char* flag_icon_path{ "./resources/icons/flag.png" };

	s_Images.Flag.D3D11Texture = LoadTextureFromFile(flag_icon_path,
		imgui_wrapper::get_d3d11_Device(), imgui_wrapper::get_d3d11_DeviceContext());

	if (s_Images.Flag.D3D11Texture == nullptr)
		return false;

	s_Images.Flag.ImGuiTexID = (ImTextureID)s_Images.Flag.D3D11Texture;
	return true;
}

bool load_mine_icon() {
	constexpr const char* mine_icon_path{ "./resources/icons/mine.png" };

	s_Images.Mine.D3D11Texture = LoadTextureFromFile(mine_icon_path,
		imgui_wrapper::get_d3d11_Device(), imgui_wrapper::get_d3d11_DeviceContext());

	if (s_Images.Mine.D3D11Texture == nullptr)
		return false;

	s_Images.Mine.ImGuiTexID = (ImTextureID)s_Images.Mine.D3D11Texture;
	return true;
}

bool load_ingame_images() {
	constexpr const char* start_page_images[5]{
		"./resources/icons/easy.png", "./resources/icons/medium.png", "./resources/icons/hard.png", "./resources/icons/expert.png", "./resources/icons/custom.png"
	};

	auto D3D11Device = imgui_wrapper::get_d3d11_Device();
	auto D3D11DeviceContntex = imgui_wrapper::get_d3d11_DeviceContext();

	for (int i{}; i < 5; i++) {
		s_Images.StartPageIcons[i].D3D11Texture = LoadTextureFromFile(start_page_images[i], D3D11Device, D3D11DeviceContntex);

		if (s_Images.StartPageIcons[i].D3D11Texture == nullptr)
			return false;

		s_Images.StartPageIcons[i].ImGuiTexID = (ImTextureID)s_Images.StartPageIcons[i].D3D11Texture;
	}

	constexpr const char* timer_icon_path{ "./resources/icons/timer.png" };
	s_Images.Clock.D3D11Texture = LoadTextureFromFile(timer_icon_path, D3D11Device, D3D11DeviceContntex);

	if (s_Images.Clock.D3D11Texture == nullptr)
		return false;

	s_Images.Clock.ImGuiTexID = (ImTextureID)s_Images.Clock.D3D11Texture;

	// Emoji Happy Image
	constexpr const char* emoji_icon_path{ "./resources/icons/emoji_happy.png" };
	s_Images.EmojiHappy.D3D11Texture = LoadTextureFromFile(emoji_icon_path, D3D11Device, D3D11DeviceContntex);

	if (s_Images.EmojiHappy.D3D11Texture == nullptr)
		return false;
	s_Images.EmojiHappy.ImGuiTexID = (ImTextureID)s_Images.EmojiHappy.D3D11Texture;

	// Emoji Sad Image
	constexpr const char* emoji_sad_icon_path{ "./resources/icons/emoji_sad.png" };
	s_Images.EmojiSad.D3D11Texture = LoadTextureFromFile(emoji_sad_icon_path, D3D11Device, D3D11DeviceContntex);

	if (s_Images.EmojiSad.D3D11Texture == nullptr)
		return false;
	s_Images.EmojiSad.ImGuiTexID = (ImTextureID)s_Images.EmojiSad.D3D11Texture;

	return true;
}

bool LoadImages() {
	if (!load_mine_icon())
		return false;
	if (!load_flag_icon())
		return false;
	if (!load_ingame_images())
		return false;

	return true;
}

bool LoadFonts() {
	constexpr const char* fonts_path{ "./resources/fonts/Aptos-Bold.ttf" };
	ImGuiIO& io = ImGui::GetIO();

	for (int i{}; i < FontsCount; i++) {
		g_Fonts[i] = io.Fonts->AddFontFromFileTTF(fonts_path, g_FontSizes[i]);
		if (!g_Fonts) return false;
	}
	io.Fonts->Build();
	io.FontDefault = g_Fonts[Larg];
	return true;
}

bool GetSaveDir(std::filesystem::path& SaveDir) {
	constexpr const char* SaveDirName{ "Aminia's MineSweeper" };

#ifdef _WIN32
	char* cstr_dir{};
	size_t size = 0;

	if (_dupenv_s(&cstr_dir, &size, "USERPROFILE") == 0 && cstr_dir != nullptr) {
		SaveDir = cstr_dir;
		SaveDir /= "Documents";
		SaveDir /= SaveDirName;

		free(cstr_dir); // Free allocated memory
		return true;
	}
	else {
		return false;
	}

#elif defined (__linux__)
	homeDir = std::filesystem::path(std::getenv("HOME"));
#else
#error "Could not find the correct OS"
#endif
}

inline bool SaveDirExists(const std::filesystem::path& SaveDir) {
	return fs::exists(SaveDir);
}

inline bool CreateSaveDir(const std::filesystem::path& SaveDir) {
	// Attempt to create the directory
	try {
		if (!std::filesystem::create_directory(SaveDir)) {
			return false;
		}
		return true;
	}
	catch (const std::filesystem::filesystem_error&) {
		return false;
	}

	return false;
}

bool LoadSaves() {
	constexpr const char* SaveFileName{ "Saves.ms" };
	std::filesystem::path SaveDir;

	if (!GetSaveDir(SaveDir))
		return false;

	// This should only happen at the very first run of the game
	if (!SaveDirExists(SaveDir)) {
		CreateSaveDir(SaveDir);

		strcpy_s(s_Stats.Name, minesweeper::PLAYER_NAME_MAX_LENGTH + 1, "Guest");
		s_Stats.Score = 0;
		for (int i{}; i < (int)minesweeper::Difficulty::Count; i++)
			s_Stats.BestTimes[i] = minesweeper::PLAYER_TIME_NA;

		return true;
	}

	std::ifstream saveFile{ SaveDir / SaveFileName };
	if (!saveFile.is_open())
		return false;

	saveFile.read(reinterpret_cast<char*>(&s_Stats), sizeof(minesweeper::PlayerStats));

	saveFile.close();
	return true;
}

bool SaveGame() {
	constexpr const char* SaveFileName{ "Saves.ms" };
	std::filesystem::path SaveDir;

	if (!GetSaveDir(SaveDir))
		return false;

	if (!SaveDirExists(SaveDir)) {
		CreateSaveDir(SaveDir);
	}

	std::ofstream saveFile{ SaveDir / SaveFileName };
	if (!saveFile.is_open())
		return false;

	saveFile.write(reinterpret_cast<char*>(&s_Stats), sizeof(minesweeper::PlayerStats));

	saveFile.close();
	return true;
}

bool InitGame() {
	if (!LoadImages())
		return false;

	ma_engine_init(NULL, &s_SoundEngine); // Use default settings
	LoadSaves();

	std::this_thread::sleep_for(sc::seconds(2));
	s_Status.IsInitialized = true;
	return true;
}

bool imgui_wrapper::USER_startup() {
	s_Status.IsInitialized = false;

	if (!LoadFonts())
		return false;

	constexpr const char* mine_icon_path{ "./resources/icons/loading.png" };
	s_Images.Loading.D3D11Texture = LoadTextureFromFile(mine_icon_path,
		imgui_wrapper::get_d3d11_Device(), imgui_wrapper::get_d3d11_DeviceContext());

	if (s_Images.Loading.D3D11Texture == nullptr)
		return false;
	s_Images.Loading.ImGuiTexID = (ImTextureID)s_Images.Loading.D3D11Texture;

	s_Pages.Loading = true;
	s_thr = std::thread(InitGame);
	
	//InitGame();
	//s_Pages.Start = true;
	 
	return true;
}

void imgui_wrapper::USER_cleanup() {
	s_Images.release();

	if (s_thr.joinable())
		s_thr.join();

	SaveGame();
}