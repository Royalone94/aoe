/* Copyright 2016-2020 the Age of Empires Free Software Remake authors. See LEGAL for legal info */

#include "ui.hpp"

#include "engine.hpp"

#include <cassert>
#include <cstdlib>
#include <cctype>

struct in_addr;

namespace genie {

namespace ui {

bool UI::contains(int x, int y) const {
	SDL_Point pt{x, y};
	return SDL_PointInRect(&pt, &bnds);
}

DynamicUI::DynamicUI(const SDL_Rect bnds[screen_modes], ConfigScreenMode mode, bool enhanced) : UI(bnds[(unsigned)mode]), scr_bnds(), enhanced(enhanced) {
	for (unsigned i = 0; i < screen_modes; ++i)
		scr_bnds[i] = bnds[i];
}

static void ui_scale(const SDL_Rect scr_bnds[screen_modes], const SDL_Rect &this_bnds, const SDL_Rect &bnds, SDL_Rect &resized)
{
	int ref_index;

	if ((ref_index = cmp_lgy_dim(bnds.w, bnds.h)) >= 0) {
		// The legacy boundaries are hardcoded anyway, so we don't need to do any fancy scaling.
		resized = scr_bnds[ref_index];
		return;
	}

	ref_index = 0;

	if (bnds.w >= 800)
		ref_index = 1;
	if (bnds.w >= 1024)
		ref_index = 2;

	const SDL_Rect &ref = scr_bnds[ref_index];
	double x0 = ref.x, y0 = ref.y, x1 = (double)ref.x + ref.w, y1 = (double)ref.y + ref.h;

	// step 1: move to center of screen
	x0 -= lgy_dim[ref_index].w * 0.5;
	x1 -= lgy_dim[ref_index].w * 0.5;
	y0 -= lgy_dim[ref_index].h * 0.5;
	y1 -= lgy_dim[ref_index].h * 0.5;

	// step 2: scale
	double sx = (double)bnds.w / lgy_dim[ref_index].w, sy = (double)bnds.h / lgy_dim[ref_index].h;
	x0 *= sx; x1 *= sx; y0 *= sy; y1 *= sy;

	// step 3: move back to origin
	x0 += bnds.w * 0.5;
	x1 += bnds.w * 0.5;
	y0 += bnds.h * 0.5;
	y1 += bnds.h * 0.5;

#pragma warning(push)
#pragma warning(disable: 4244)

	resized.x = x0;
	resized.y = y0;
	resized.w = x1 - x0;
	resized.h = y1 - y0;

#pragma warning(pop)
}

void DynamicUI::resize(ConfigScreenMode old_mode, ConfigScreenMode mode) {
	// Figure out what reference we have to use
	auto &r = eng->w->render();
	auto &old_dim = r.old_dim, &dim = r.dim;
	SDL_Rect old_bnds = old_dim.lgy_orig, bnds = enhanced ? dim.rel_bnds : dim.lgy_orig;
	SDL_Rect resized;
	ui_scale(scr_bnds, this->bnds, bnds, resized);

	this->bnds = resized;
}

Border::Border(const SDL_Rect bnds[screen_modes], ConfigScreenMode mode, const Palette &pal, const BackgroundSettings &bkg, BorderType type, bool enhanced) : DynamicUI(bnds, mode, enhanced), cols(), type(type), shade(0), flip(false) {
	for (unsigned i = 0; i < 6; ++i)
		cols[i] = pal.tbl[bkg.bevel[i]];

	switch (type) {
	case BorderType::background:
		shade = 0;
		break;
	case BorderType::button:
	case BorderType::field:
		shade = 2 * bkg.shade;
		break;
	}
}

void Border::paint(SimpleRender &r) {
	r.border(bnds, cols, shade, flip);
}

void Border::paint(SimpleRender &r, const SDL_Color &bg) {
	r.border(bnds, cols, bg, flip);
}

Label::Label(SimpleRender &r, Font &f, const std::string &s, SDL_Color fg, SDL_Color bg, const SDL_Rect bnds[screen_modes], ConfigScreenMode mode, HAlign halign, VAlign valign, bool adjust_anchors, bool enhanced)
	: DynamicUI(bnds, mode, enhanced)
	, fg(fg), bg(bg), txt_fg(r, f, s, fg), txt_bg(r, f, s, bg), halign(halign), valign(valign)
{
	auto &tex = txt_fg.tex();

	// Update width and height of each boundary, since we have no guarantee that these match with the created texture
	for (unsigned i = 0; i < screen_modes; ++i) {
		scr_bnds[i].w = tex.width;
		scr_bnds[i].h = tex.height;
	}

	if (adjust_anchors) {
		for (unsigned i = 0; i < screen_modes; ++i) {
			switch (halign) {
			case HAlign::center:
				scr_bnds[i].x -= tex.width / 2;
				break;
			case HAlign::right:
				scr_bnds[i].x -= tex.width;
				break;
			}

			switch (valign) {
			case VAlign::middle:
				scr_bnds[i].y -= tex.height / 2;
				break;
			case VAlign::bottom:
				scr_bnds[i].y -= tex.height;
				break;
			}
		}

		switch (halign) {
		case HAlign::center:
			this->bnds.x -= tex.width / 2;
			break;
		case HAlign::right:
			this->bnds.x -= tex.width;
			break;
		}

		switch (valign) {
		case VAlign::middle:
			this->bnds.y -= tex.height / 2;
			break;
		case VAlign::bottom:
			this->bnds.y -= tex.height;
			break;
		}
	}

	this->bnds.w = tex.width;
	this->bnds.h = tex.height;
}

Label::Label(SimpleRender &r, Font &f, const std::string &s, const SDL_Rect bnds[screen_modes], ConfigScreenMode mode, const Palette &pal, const BackgroundSettings &bkg, HAlign halign, VAlign valign, bool adjust_anchors, bool enhanced)
	: Label(r, f, s, {bkg.text[0], bkg.text[1], bkg.text[2], 0xff}, {bkg.text[3], bkg.text[4], bkg.text[5], 0xff}, bnds, mode, halign, valign, adjust_anchors, enhanced)
{
	resize(mode, mode);
}

// Resize slightly differs from base implementation: width and height always remain the same, even if that means it will draw out of bounds
void Label::resize(ConfigScreenMode old_mode, ConfigScreenMode mode) {
	// Remember old width and height
	int w = scr_bnds[0].w, h = scr_bnds[0].h;

	for (unsigned i = 0; i < screen_modes; ++i) {
		scr_bnds[i].w = w;
		scr_bnds[i].h = h;
	}

	// Let base do naive resizing and correct alignment afterwards
	DynamicUI::resize(old_mode, mode);

	// Ensure w and h haven't changed
	if (!mode_is_legacy(mode)) {
		// mode was either fullscreen or custom, now we have to re-align the new dimensions
		int dw = bnds.w - w, dh = bnds.h - h;

		switch (halign) {
		case HAlign::left:
			break;
		case HAlign::center:
			bnds.x += dw / 2;
			break;
		case HAlign::right:
			bnds.x += dw;
			break;
		}

		switch (valign) {
		case VAlign::top:
			break;
		case VAlign::middle:
			bnds.y += dh / 2;
			break;
		case VAlign::bottom:
			bnds.y += dh;
			break;
		}
	}

	// Ignore new width and height (text is never scaled, because it will look really bad)
	this->bnds.w = w;
	this->bnds.h = h;
}

void Label::paint(SimpleRender &r) {
	txt_bg.paint(r, bnds.x - 1, bnds.y + 1);
	txt_fg.paint(r, bnds.x, bnds.y);
}

Button::Button(unsigned id, InteractableCallback &cb, SimpleRender &r, const SDL_Rect bnds[screen_modes], ConfigScreenMode mode, const Palette &pal, const BackgroundSettings &bkg, DynamicUI *decorator, bool enhanced)
	: Border(bnds, mode, pal, bkg, BorderType::button, enhanced), Interactable(id, cb)
	, decorator(decorator)
{
	resize(mode, mode);
}

Button::Button(unsigned id, InteractableCallback &cb, SimpleRender &r, Font &f, const std::string &s, SDL_Color fg, SDL_Color bg, const SDL_Rect txt_bnds[screen_modes], const SDL_Rect bnds[screen_modes], const Palette &pal, const BackgroundSettings &bkg, ConfigScreenMode mode, HAlign halign, VAlign valign, bool adjust_anchors, bool enhanced)
	: Button(id, cb, r, bnds, mode, pal, bkg, new Label(r, f, s, fg, bg, txt_bnds, mode, halign, valign, adjust_anchors, enhanced), enhanced) {}

void Button::resize(ConfigScreenMode old_mode, ConfigScreenMode mode) {
	Border::resize(old_mode, mode);
	decorator->resize(old_mode, mode);
}

void Button::paint(SimpleRender &r) {
	Border::paint(r);
	decorator->paint(r);
}

void Button::focus(bool on) {
	has_focus = on;
}

void Button::press(bool on) {
	flip = on;

	if (is_pressed && !on) {
		is_pressed = false;
		cb.interacted(int_id);
	}

	is_pressed = on;
}

InputField::InputField(unsigned id, InputCallback &cb, InputType type, const std::string &init, SimpleRender &r, Font &f, SDL_Color fg, const SDL_Rect bnds[screen_modes], ConfigScreenMode mode, const Palette &pal, const BackgroundSettings &bkg, bool enhanced)
	: Border(bnds, mode, pal, bkg, BorderType::field, enhanced)
	, TextBuf(r, f, init, fg)
	, index(id), type(type), cb(cb), hasfocus(false), error(false)
{
	resize(mode, mode);
}

bool InputField::keydown(int ch) {
	if (!hasfocus)
		return false;

	if (ch == '\b' || ch == SDLK_BACKSPACE) {
		TextBuf::erase();
		return true;
	}

	if (ch == '\n' || ch == '\r')
		return true;

	switch (type) {
	case InputType::number:
	case InputType::port:
	case InputType::ip:
		if (ch >= '0' && ch <= '9') {
			if (type == InputType::port)
				if (TextBuf::str().length() >= 5)
					return true;

			TextBuf::append(ch);
		} else if (type == InputType::number && (ch == '+' || ch == '-')) {
			TextBuf::append(ch);
		} else if (type == InputType::ip && ch == '.') {
			TextBuf::append(ch);
		} else {
			return false;
		}
		break;
	case InputType::text:
		if (ch >= 0 && ch < 0xff && isprint(ch))
			TextBuf::append(ch);
		else
			return false;
		break;
	}

	return true;
}

bool InputField::keyup(int ch) {
	if (!hasfocus)
		return false;

	if (ch == '\n' || ch == '\r') {
		if (cb.input(index, *this))
			TextBuf::clear();
		return true;
	}

	switch (type) {
	case InputType::number:
	case InputType::port:
	case InputType::ip:
		if (ch >= '0' && ch <= '9')
			return true;
		if (type == InputType::number && (ch == '+' || ch == '-'))
			return true;
		if (type == InputType::ip && ch == '.')
			return true;
		break;
	case InputType::text:
		if (ch >= 0 && ch < 0xff && isprint(ch))
			return true;
		break;
	}

	return false;
}

void InputField::focus(bool on) {
	hasfocus = on;
}

long long InputField::number() const {
	assert(type == InputType::number);
	return atoll(str().c_str());
}

uint16_t InputField::port() const {
	assert(type == InputType::port);
	return atoi(str().c_str());
}

bool InputField::ip(uint32_t &ip) const {
	assert(type == InputType::ip);
	return str_to_ip(str(), ip);
}

const std::string &InputField::str() const {
	return TextBuf::str();
}

const std::string &InputField::text() const {
	assert(type == InputType::text);
	return str();
}

void InputField::paint(SimpleRender& r) {
	if (hasfocus)
		Border::paint(r, {0xff, 0xff, 0xff, (uint8_t)shade});
	else if (error)
		Border::paint(r, {0xff, 0, 0, (uint8_t)shade});
	else
		Border::paint(r);

	auto *t = TextBuf::txt.get();
	int h = t ? t->tex().height : 0;
	TextBuf::paint(Border::bnds.x + 8, Border::bnds.y + (Border::bnds.h - h) / 2);
}

}
}
