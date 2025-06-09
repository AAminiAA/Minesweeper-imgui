#include "MineSweeper.h"
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <algorithm>
#include <random>
#include <thread>

time_t seed{};

MineSweeper_NS_Begin

MineSweeper::MineSweeper(const Difficulty _diff)
	: __grid{ (size_t)MineSweeper::s_Preset_Grid_sizes[(size_t)_diff].y, Grid_row((size_t)MineSweeper::s_Preset_Grid_sizes[(size_t)_diff].x, {Cell_State::Unsweeped, 0}) },
	__remaining_bombs{}, __remaining_cells{}, __exploded_bombs{}, __bombs_count{}, __flagged_count{}, __diff {_diff}, __is_initialized{}, __is_game_over{}
{
	const auto& grid_size = MineSweeper::s_Preset_Grid_sizes[(size_t)__diff];
	const auto& bomb_ratio = MineSweeper::s_Preset_Bomb_ratio[(size_t)__diff];

	__bombs_count = round(grid_size.x * grid_size.y * bomb_ratio);
	__remaining_bombs = __bombs_count;

	__remaining_cells = grid_size.x * grid_size.y;
}

void MineSweeper::_place_bombs(const Pos& start_pos) {
	Cell_Value placed_bombs = 0;
	seed = time(NULL);
	srand(seed);
	//srand(0);
	const Pos grid_size{ width(), height() };

	auto is_adjacent_to_start = [&start_pos](const Pos& pos) {
		if ((pos.x <= start_pos.x + 1) && (pos.x >= start_pos.x - 1) && (pos.y <= start_pos.y + 1) && (pos.y >= start_pos.y - 1))
			return true;

		return false;
		};

	while (__bombs_count != placed_bombs) {
		Pos bomb_pos{ rand() % grid_size.x, rand() % grid_size.y };
		// Start pos must be 0
		if (is_bomb(bomb_pos) || is_adjacent_to_start(bomb_pos))
			continue;

		__grid.at(bomb_pos.y).at(bomb_pos.x).value = s_BOMB;
		placed_bombs++;
	}
}

void MineSweeper::_calculate_adjacent_bombs(const Pos& cell_pos) {
	auto& cell = __grid.at(cell_pos.y).at(cell_pos.x);
	cell.value = 0;

	for (Cell_Value i{}; i < 3; i++) {
		if (cell_pos.y - 1 + i < 0 || cell_pos.y - 1 + i >= __grid.size())
			continue;
		for (Cell_Value j{}; j < 3; j++) {
			if (cell_pos.x - 1 + j < 0 || cell_pos.x - 1 + j >= __grid[0].size())
				continue;

			if (is_bomb({ cell_pos.x - 1 + j, cell_pos.y - 1 + i }))
				cell.value++;
		}
	}
}

void MineSweeper::_sweep_zeros(const Pos& cell_pos)
{
	auto& cell = __grid.at(cell_pos.y).at(cell_pos.x);

	for (Cell_Value i{}; i < 3; i++) {
		auto current_cell_row = cell_pos.y - 1 + i;
		if (current_cell_row < 0 || current_cell_row >= __grid.size())
			continue;
		for (Cell_Value j{}; j < 3; j++) {
			auto current_cell_col = cell_pos.x - 1 + j;
			if (current_cell_col < 0 || current_cell_col >= __grid[0].size())
				continue;

			auto& adj_cell = __grid.at(current_cell_row).at(current_cell_col);

			if (adj_cell.value == 0 && adj_cell.state == Cell_State::Unsweeped) {
				adj_cell.state = Cell_State::Sweeped;
				__remaining_cells--;
				//_sweep_all_adjacent({ current_cell_col, current_cell_row });
				_sweep_zeros({ current_cell_col, current_cell_row });
			}
			else if (cell.value == 0 && adj_cell.state == Cell_State::Unsweeped) {
				adj_cell.state = Cell_State::Sweeped;
				__remaining_cells--;
			}
		}
	}
}

void MineSweeper::_sweep_all_adjacent(const Pos& cell_pos)
{
	for (Cell_Value i{}; i < 3; i++) {
		auto current_cell_row = cell_pos.y - 1 + i;
		if (current_cell_row < 0 || current_cell_row >= __grid.size())
			continue;
		for (Cell_Value j{}; j < 3; j++) {
			auto current_cell_col = cell_pos.x - 1 + j;
			if (current_cell_col < 0 || current_cell_col >= __grid[0].size())
				continue;

			auto& cell = __grid.at(current_cell_row).at(current_cell_col);
			cell.state = Cell_State::Sweeped;
		}
	}
}

// Re-Playing the current game without changing it
void MineSweeper::restart_game()
{
	const auto& grid_size = MineSweeper::s_Preset_Grid_sizes[(size_t)__diff];

	__remaining_bombs = __bombs_count;
	__exploded_bombs = 0;
	__flagged_count = 0;
	__remaining_cells = grid_size.x * grid_size.y;
	__is_initialized = true;
	__is_game_over = false;

	for (auto& row : __grid) {
		for (auto& cell : row) {
			cell.state = Cell_State::Unsweeped;
		}
 	}

	clear_timer();
}

void MineSweeper::new_game()
{
	__remaining_bombs = __bombs_count;
	__exploded_bombs = 0;
	__flagged_count = 0;
	__remaining_cells = height() * width();
	__is_initialized = false;
	__is_game_over = false;

	for (auto& row : __grid) {
		for (auto& cell : row) {
			cell.state = Cell_State::Unsweeped;
			cell.value = 0;
		}
	}

	clear_timer();
}

void MineSweeper::new_game(const Difficulty _diff)
{
	__diff = _diff;
	const auto& grid_size = MineSweeper::s_Preset_Grid_sizes[(size_t)__diff];
	const auto& bomb_ratio = MineSweeper::s_Preset_Bomb_ratio[(size_t)__diff];

	__bombs_count = round(grid_size.x * grid_size.y * bomb_ratio);
	__remaining_bombs = __bombs_count;
	__exploded_bombs = 0;
	__flagged_count = 0;
	__remaining_cells = grid_size.x * grid_size.y;
	__is_initialized = false;
	__is_game_over = false;

	__grid = Grid{ (size_t)MineSweeper::s_Preset_Grid_sizes[(size_t)_diff].y, Grid_row((size_t)MineSweeper::s_Preset_Grid_sizes[(size_t)_diff].x, {Cell_State::Unsweeped, 0}) };
	clear_timer();
}

void MineSweeper::new_game(const size_t row, const size_t col, const uint16_t mines_count)
{
	__diff = Difficulty::Custom;

	__bombs_count = mines_count;
	__remaining_bombs = __bombs_count;
	__exploded_bombs = 0;
	__flagged_count = 0;
	__remaining_cells = row * col;
	__is_initialized = false;
	__is_game_over = false;

	__grid = Grid{ row, Grid_row(col, {Cell_State::Unsweeped, 0}) };
	clear_timer();
}

void MineSweeper::randomly_flag_mine_count()
{
	__flagged_count = 0;

	const Pos grid_size{ width(), height() };

	while (__bombs_count != __flagged_count) {
		Pos flag_pos{ rand() % grid_size.x, rand() % grid_size.y };
		auto& cell = __grid.at(flag_pos.y).at(flag_pos.x);
		// Start pos must be 0
		if (cell.is_marked())
			continue;

		cell.state = Cell_State::Marked;
		__flagged_count++;
	}
}

void MineSweeper::clear_flags()
{
	for (auto& row: __grid) {
		for (auto& cell : row) {
			if (cell.is_marked())
				cell.state = Cell_State::Unsweeped;
		}
	}

	__flagged_count = 0;
}

void MineSweeper::_print(bool cheat_on)
{
	std::cout << ">> Seed: " << seed << '\n';
	std::cout << ">> Remaining bombs: " << __remaining_bombs << '\n';
	std::cout << ">> Remaining cells: " << __remaining_cells << '\n';
	std::cout << "    ";
	for (size_t i = 0; i < __grid.at(0).size(); i++)
	{
		std::cout << " " << i << " ";
	}
	std::cout << '\n' << std::string(3 * (__grid.at(0).size() + 2), '-') << '\n';

	size_t row_count{};
	for (const auto& row : __grid) {
		std::cout << " " << row_count << " |";
		for (const auto& cell : row) {
			if (!cheat_on) {
				if (cell.state == Cell_State::Sweeped) {
					if (cell.value == s_BOMB)
						std::cout << " " << (char)cell.value << " ";
					else
						std::cout << " " << cell.value << " ";
				}
				else {
					std::cout << "[ ]";
				}
			}
			else {
				if (cell.value == s_BOMB)
					std::cout << " " << (char)cell.value << " ";
				else
					std::cout << " " << cell.value << " ";
			}

		}
		std::cout << "| " << row_count << " ";
		row_count++;
		std::cout << '\n';
	}

	std::cout << std::string(3 * (__grid.at(0).size() + 2), '-') << '\n';
	std::cout << "    ";
	for (size_t i = 0; i < __grid.at(0).size(); i++)
	{
		std::cout << " " << i << " ";
	}
	
	std::cout << std::endl;
}

void MineSweeper::_initiailize_grid(const Pos& start_pos)
{
	_place_bombs(start_pos);

	for (Cell_Value row{}; row < __grid.size(); row++) {
		auto& current_row = __grid[row];

		for (Cell_Value col{}; col < current_row.size(); col++) {
			if (!is_bomb({ col,row }))
				_calculate_adjacent_bombs({ col,row });
		}
	}

	__is_initialized = true;
}

bool MineSweeper::sweep(const Pos& cell_pos)
{
	if (!__is_initialized)
		_initiailize_grid(cell_pos);

	auto& cell = __grid.at(cell_pos.y).at(cell_pos.x);
	if (cell.state == Cell_State::Unsweeped) {
		cell.state = Cell_State::Sweeped;
		__remaining_cells--;
		_sweep_zeros(cell_pos);

		__is_game_over = cell.is_bomb();
		// This is useful when game might be revived
		if (__is_game_over) {
			__remaining_bombs--;
			__exploded_bombs++;
			__flagged_count++;
		}
		return !__is_game_over;
	}

	return true;
}

bool MineSweeper::sweep(const Cell_Value row, const Cell_Value col)
{
	return sweep({ col, row });
}

void MineSweeper::mark(const Pos& cell_pos) {
	if (__flagged_count >= __bombs_count)
		return;

	auto& cell = __grid.at(cell_pos.y).at(cell_pos.x);
	if (__remaining_bombs == 0 || cell.is_sweeped())
		return;

	__remaining_bombs--;
	__flagged_count++;
	cell.state = Cell_State::Marked;
}

void MineSweeper::unmark(const Pos& cell_pos)
{
	auto& cell = __grid.at(cell_pos.y).at(cell_pos.x);
	if (!cell.is_marked())
		return;

	__remaining_bombs++;
	__flagged_count--;
	cell.state = Cell_State::Unsweeped;
}

void MineSweeper::toggle_mark(const Pos& cell_pos)
{
	auto& cell = __grid.at(cell_pos.y).at(cell_pos.x);

	if (cell.state == Cell_State::Marked) {
		__remaining_bombs++;
		__flagged_count--;
		cell.state = Cell_State::Unsweeped;
	}
	else if (cell.state == Cell_State::Unsweeped && __flagged_count < __bombs_count) {
		__remaining_bombs--;
		__flagged_count++;
		cell.state = Cell_State::Marked;
	}
}

void MineSweeper::toggle_mark(const Cell_Value row, const Cell_Value col)
{
	toggle_mark({ col, row });
}

void MineSweeper::reveal_bombs()
{
	for (auto& row : __grid) {
		for (auto& cell : row) {
			if (cell.is_bomb() && !cell.is_marked())
				cell.reveal();
		}
	}
}

void MineSweeper::reveal_bombs_timer(const sc::milliseconds time_ms)
{
	Cell** all_mines = new Cell*[__bombs_count];
	int array_length = 0;

	for (int row{}; row < __grid.size(); row++) {
		for (int col{}; col < __grid[0].size(); col++) {
			Cell* cell = &__grid[row][col];
			if (cell->is_bomb()) {
				if(array_length < __bombs_count)
					all_mines[array_length] = cell;
				array_length++;
			}
		}
	}

	int* all_mines_indexes = new int[__bombs_count];
	for (int i{}; i < __bombs_count; i++)
		all_mines_indexes[i] = i;

	std::random_device rd;
	std::mt19937 g(rd());

	std::shuffle(all_mines_indexes, all_mines_indexes + __bombs_count, g);

	for (int i{}; i < __bombs_count; i++) {
		all_mines[all_mines_indexes[i]]->reveal();
		std::this_thread::sleep_for(time_ms / __bombs_count);
	}

	delete[] all_mines;
	delete[] all_mines_indexes;
}


MineSweeper_NS_End