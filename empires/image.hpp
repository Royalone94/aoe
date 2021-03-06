/* Copyright 2016-2019 the Age of Empires Free Software Remake authors. See LEGAL for legal info */

/*
 * Anything image and animation related stuff goes here
 */

#pragma once

#include "drs.h"

#include <SDL2/SDL_image.h>
#include <memory>

/**
 * Holds all current available indexed colors.
 */
class Palette final {
public:
	// XXX wrap in unique_ptr?
	SDL_Palette *pal;

	Palette() : pal(NULL) {}
	Palette(unsigned id) : pal(NULL) { open(id); }

	~Palette() {
		if (pal)
			SDL_FreePalette(pal);
	}

	void open(unsigned id);
	void set_border_color(unsigned id, unsigned col_id) const;
};

/**
 * SLP wrapper.
 */
class Image final {
public:
	std::shared_ptr<SDL_Surface> surface;
	std::shared_ptr<SDL_Texture> texture;
	int hotspot_x, hotspot_y;

	Image();

	bool load(
		Palette *pal, const void *data,
		const struct slp_frame_info *frame,
		unsigned player = 0
	);

	void draw(int x, int y, unsigned w=0, unsigned h=0) const;
};

/**
 * SLP image wrapper.
 */
class AnimationTexture final {
	struct slp image;
public:
	// FIXME make private and wrap in unique_ptr
	std::unique_ptr<Image[]> images;
	bool dynamic;
	unsigned id; // FIXME debug stuff, remove when done

	AnimationTexture() : images(), dynamic(false), id(0) {}
	AnimationTexture(Palette *pal, unsigned id) : images(), dynamic(false), id(id) { open(pal, id); }

	void open(Palette *pal, unsigned id);
	void draw(int x, int y, unsigned index, unsigned player = 0) const;
	void draw(int x, int y, unsigned w, unsigned h, unsigned index, unsigned player = 0) const;
	void draw_selection(int x, int y, unsigned size) const;
};
