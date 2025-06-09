#pragma once
#include <stdint.h>
#include <vector>
#include <chrono>

namespace sc = std::chrono;

#define MiSw minesweeper::
#define MiSw_NS minesweeper

#define MineSweeper_NS_Begin	namespace MiSw_NS {
#define MineSweeper_NS_End		}

MineSweeper_NS_Begin

class MineSweeper;
typedef typename int Cell_Value;

enum class Cell_State
{
	Unsweeped,
	Sweeped,
	Marked,

	Bomb = '*'
};

struct Cell
{
	Cell_State state;
	Cell_Value value;

	bool is_bomb() const { return value == (Cell_Value)Cell_State::Bomb; }
	bool is_sweeped() const { return state == Cell_State::Sweeped; }
	bool is_marked() const { return state == Cell_State::Marked; }

	void reveal() { state = Cell_State::Sweeped; }
};

typedef typename std::vector<Cell> Grid_row;
typedef typename std::vector<Grid_row> Grid;

enum class Difficulty
{
	Easy,
	Medium,
	Hard,
	Expert,

	Count,

	Custom
};

struct Pos
{
	Cell_Value x;
	Cell_Value y;

	bool operator==(const Pos& rhs) {
		return (x == rhs.x && y == rhs.y);
	}
};

inline constexpr int PLAYER_NAME_MAX_LENGTH{ 20 };
inline constexpr Cell_Value PLAYER_TIME_NA{ -1 };

#pragma pack(push, 1)
struct PlayerStats {
	char Name[PLAYER_NAME_MAX_LENGTH + 1]{};

	Cell_Value BestTimes[(size_t)Difficulty::Count];
	Cell_Value BestNoFlagTimes[(size_t)Difficulty::Count];
	Cell_Value BestNoMistakeTimes[(size_t)Difficulty::Count];
	Cell_Value Score{};
};
#pragma pack(pop)

class MineSweeper
{
public:
	using Time_t = double;
	typedef Time_t(*Fptr_TimerHandler)();

public:
	static constexpr Cell_Value s_BOMB{ '*' };
	static constexpr Pos s_Preset_Grid_sizes[(size_t)Difficulty::Count]{ {9,9}, {16,16}, {30,16}, {30,20} };

private:
	static constexpr const char* s_Difficulty_str[(size_t)Difficulty::Count]{ "Easy", "Medium", "Hard", "Expert" };
	// #of cells to #of bombs
	static constexpr float s_Preset_Bomb_ratio[(size_t)Difficulty::Count]{ 0.11f, 0.156f, 0.206f, 0.241f };

private:
	Grid __grid;
	Cell_Value __remaining_bombs;
	Cell_Value __exploded_bombs;
	Cell_Value __remaining_cells;
	Cell_Value __bombs_count;
	Cell_Value __flagged_count;
	Difficulty __diff;

	bool __is_initialized;
	bool __is_game_over;

	Time_t __start_time;
	Time_t __elapsed_time;
	bool __timer_running;

	Fptr_TimerHandler GetTime;
public:
	MineSweeper(const Difficulty _diff);
	MineSweeper(const Cell_Value rows, const Cell_Value cols, const Cell_Value mines);
	MineSweeper() = delete;

public:
	// returns true if successful, false if cell was bomb
	bool sweep(const Pos& cell);
	bool sweep(const Cell_Value row, const Cell_Value col);

	void mark(const Pos& cell);
	void mark(const Cell_Value row, const Cell_Value col);

	void unmark(const Pos& cell);
	void unmark(const Cell_Value row, const Cell_Value col);

	void toggle_mark(const Pos& cell);
	void toggle_mark(const Cell_Value row, const Cell_Value col);

	void reveal_bombs();
	void reveal_bombs_timer(const sc::milliseconds time_ms);

	Cell_State get_state(const Pos cell) const { return __grid.at(cell.y).at(cell.x).state; }
	Cell_Value get_value(const Pos cell) const { return __grid.at(cell.y).at(cell.x).value; }

	const Cell& get_cell(const Pos cell_pos) { return __grid.at(cell_pos.y).at(cell_pos.x); }
	const Cell& get_cell(const Cell_Value row, const Cell_Value col) const { return __grid.at(row).at(col); }
	Cell& get_cell(const Cell_Value row, const Cell_Value col) { return __grid.at(row).at(col); }

	const char* get_diff_str() const { return (__diff < Difficulty::Count && __diff >= Difficulty(0)) ? s_Difficulty_str[(size_t)__diff] : "Custom"; }
	Pos get_grid_size() const { return (__diff < Difficulty::Count && __diff >= Difficulty(0)) ? s_Preset_Grid_sizes[(size_t)__diff] : Pos{ (Cell_Value)__grid.at(0).size(), (Cell_Value)__grid.size() }; }

	Cell_Value height() const { return __grid.size(); }
	Cell_Value width() const { return __grid.at(0).size(); }

	Cell_Value get_remaining_bombs() const { return __remaining_bombs; }
	Cell_Value get_mine_count() const { return __bombs_count; }
	Difficulty get_difficulty() const { return __diff; }
	Cell_Value get_exploded_mines() const { return __exploded_bombs; }

	bool is_bomb(const Pos cell) const { return __grid.at(cell.y).at(cell.x).value == s_BOMB; }
	bool is_bomb(const Cell_Value row, const Cell_Value col) const { return __grid.at(row).at(col).value == s_BOMB; }
	bool is_game_won() const { return (__remaining_cells + __exploded_bombs == __bombs_count); }
	bool is_game_over() const { return __is_game_over; }

	// Restart the same game
	void restart_game();
	// new game with the same different difficulty
	void new_game();
	// new game with different difficulty
	void new_game(const Difficulty _diff);
	void new_game(const size_t row, const size_t col, const uint16_t mines_count);
	void revive_game() { __is_game_over = false; }

	// Randomly falgs cells by the number of mines
	void randomly_flag_mine_count();
	void clear_flags();

	void init_timer(const Time_t time_point) { __start_time = time_point; __elapsed_time = 0;  __timer_running = false; }
	void start_timer() { __timer_running = true; }
	void stop_timer() { __timer_running = false; }
	void update_timer(const Time_t end_point) { if (__timer_running) __elapsed_time = end_point - __start_time; }
	Time_t get_time() const { return __elapsed_time; }
	void clear_timer() { __start_time = 0; __elapsed_time = 0; __timer_running = false; }

public:
	void _print(bool cheat_on);

private:
	void _initiailize_grid(const Pos& start_pos);
	void _place_bombs(const Pos& start_pos);
	void _calculate_adjacent_bombs(const Pos& cell);
	void _sweep_zeros(const Pos& pos);
	void _sweep_all_adjacent(const Pos& pos);
};

MineSweeper_NS_End