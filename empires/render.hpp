/* Copyright 2016-2019 the Age of Empires Free Software Remake authors. See LEGAL for legal info */

#pragma once

#include <SDL2/SDL.h>

#include <stack>

#include <genie/gfx.h>

class RendererState final {
public:
	float view_x, view_y;
	SDL_Color col_text, col_text_f;

	RendererState() : view_x(0), view_y(0) {
		col_text.r = 225;
		col_text.g = 208;
		col_text.b = 157;
		col_text.a = 255;
		col_text_f.r = col_text_f.g = col_text_f.a = 255;
		col_text_f.b = 0;
	}

	RendererState(float view_x, float view_y)
		: view_x(view_x), view_y(view_y) {}

	void move_view(float dx, float dy) {
		view_x += dx;
		view_y += dy;
	}

	void set_view(float vx, float vy) {
		view_x = vx;
		view_y = vy;
	}
};

class Renderer final {
	SDL_Surface *capture;
	SDL_Texture *tex;
	void *pixels;
	size_t pixels_count;
	std::stack<RendererState> state;
public:
	SDL_Renderer *renderer;
	unsigned shade;

	Renderer();
	~Renderer();

	void col(int grayvalue) {
		col(grayvalue, grayvalue, grayvalue);
	}

	void col(int r, int g, int b, int a = SDL_ALPHA_OPAQUE) {
		SDL_SetRenderDrawColor(renderer, r, g, b, a);
	}

	void col(const SDL_Color &col) {
		SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
	}

	void draw(SDL_Texture *tex, SDL_Surface *surf, int x, int y, unsigned w=0, unsigned h=0);
	void draw_selection(int x, int y, unsigned size);

	void draw_rect(int x0, int y0, int x1, int y1);
	void fill_rect(int x0, int y0, int x1, int y1);

	// NOTE SLOW
	void draw_text(int x, int y, const char *str
		, Halign halign=LEFT
		, Valign valign=TOP
		, SDL_Texture *fnt=fnt_tex_default);

	void draw_text(int x, int y, unsigned id
		, Halign halign=LEFT
		, Valign valign=TOP
		, SDL_Texture *fnt=fnt_tex_default);

	void save_screen();
	void read_screen();
	void dump_screen();

	void clear();

	void push_state() {
		state.emplace();
	}

	void push_state(RendererState &s) {
		state.push(s);
	}

	void pop_state() {
		state.pop();
	}

	RendererState &get_state() {
		return state.top();
	}
};

extern Renderer canvas;

void canvas_dirty();
