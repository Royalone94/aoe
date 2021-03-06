/* Copyright 2018-2019 the Age of Empires Free Software Remake authors. See LEGAL for legal info */

#include <genie/sfx.h>

#include <limits.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <map>
#include <memory>

#include "engine.h"

#include <genie/dbg.h>
#include <genie/cfg.h>
#include <genie/def.h>
#include <genie/fs.h>
#include <genie/drs.h>

Mix_Chunk *music_chunk = NULL;

#ifndef _WIN32
ALCdevice *alc_dev = NULL;
ALCcontext *alc_ctx = NULL;
ALuint al_src[SFX_CHANNEL_COUNT];
#endif

class Clip {
protected:
	unsigned id;

public:
	Clip(unsigned id) : id(id) {}
	virtual ~Clip() {}

	virtual void play() = 0;

	friend bool operator<(const Clip &lhs, const Clip &rhs) { return lhs.id < rhs.id; }
	friend bool operator>(const Clip &lhs, const Clip &rhs) { return lhs.id > rhs.id; }
	friend bool operator<=(const Clip &lhs, const Clip &rhs) { return lhs.id <= rhs.id; }
	friend bool operator>=(const Clip &lhs, const Clip &rhs) { return lhs.id >= rhs.id; }
	friend bool operator==(const Clip &lhs, const Clip &rhs) { return lhs.id == rhs.id; }
	friend bool operator!=(const Clip &lhs, const Clip &rhs) { return lhs.id == rhs.id; }
};

// Backed by real file (e.g. button4.wav)
class ClipFile final : public Clip {
	Mix_Chunk *chunk;
public:
	ClipFile(unsigned id, Mix_Chunk *chunk) : Clip(id), chunk(chunk) {}

	virtual ~ClipFile() {
		Mix_FreeChunk(chunk);
	}

	virtual void play() override {
		// FIXME use openal for playback
		// or make sure it gets resampled to the proper audio bit rate...
		Mix_PlayChannel(-1, chunk, 0);
	}
};

class ClipMusicFile final : public Clip {
	Mix_Music *music;
public:
	ClipMusicFile(unsigned id, Mix_Music *music) : Clip(id), music(music) {}

	virtual ~ClipMusicFile() {
		Mix_FreeMusic(music);
	}

	virtual void play() override {
		Mix_PlayMusic(music, 0);
	}
};

std::map<unsigned, std::shared_ptr<Clip>> sfx;
/**
 * Currently playing music. We don't cache music files because only one
 * music track can play at any time and it may affect I/O performance if
 * loaded from a real CD-ROM.
 */
std::shared_ptr<ClipMusicFile> music;

void sfx_init_al(void)
{
#ifndef _WIN32
	const char *dname = NULL;
	char buf[256];
	const char *msg = "unknown";

	if (alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT"))
		dname = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
	alc_dev = alcOpenDevice(dname);
	if (!alc_dev) {
		msg = "Could not open device";
		goto fail;
	}
	alc_ctx = alcCreateContext(alc_dev, NULL);
	if (!alc_ctx) {
		msg = "Could not create context";
		goto fail;
	}
	if (!alcMakeContextCurrent(alc_ctx)) {
		msg = "Cannot attach audio context";
		goto fail;
	}
	memset(al_src, 0, sizeof al_src);
	alGenSources(SFX_CHANNEL_COUNT, al_src);

	return;
fail:
	snprintf(buf, sizeof buf, "OpenAL sound failed: %s\n", msg);
	panic(buf);
#endif
}

void sfx_free_al(void)
{
#ifndef _WIN32
	alDeleteSources(SFX_CHANNEL_COUNT, al_src);
	alcDestroyContext(alc_ctx);
	alcCloseDevice(alc_dev);
#endif
}

void sfx_load(unsigned id)
{
	char fsbuf[FS_BUFSZ];
	Mix_Chunk *data;

	// try absolute paths first for special sound effects
	switch (id) {
	case SFX_BUTTON4:
		fs_help_path(fsbuf, sizeof fsbuf, "button4.wav");
		if (data = Mix_LoadWAV(fsbuf)) {
			sfx.emplace(id, std::shared_ptr<Clip>(new ClipFile(id, data)));
			return;
		}
		break;
	}

	// not found, try the drs files
	size_t n;
	const void *drs = drs_get_item(DT_WAVE, id, &n);

	if (!drs) {
		fprintf(stderr, "sfx_load: error loading %u\n", id);
		return;
	}

	if (!(data = Mix_LoadWAV_RW(SDL_RWFromMem((void*)drs, n), 1)))
		fprintf(stderr, "sfx_load: error loading %u\n", id);
	else
		sfx.emplace(id, std::shared_ptr<Clip>(new ClipFile(id, data)));
}

extern "C" {

volatile int music_playing = 0;

void hook_finished()
{
	music_playing = 0;
	dbgs("sfx: music stop");
}

void sfx_init(void)
{
	char buf[256];

	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) == -1) {
		snprintf(buf, sizeof buf, "SDL2 mixer failed to open audio: %s\n", Mix_GetError());
		panic(buf);
	}
	if (Mix_AllocateChannels(SFX_CHANNEL_COUNT) < SFX_CHANNEL_COUNT) {
		snprintf(buf, sizeof buf, "SDL2 mixer failed to allocate %d channels: %s\n", SFX_CHANNEL_COUNT, Mix_GetError());
		panic(buf);
	}
	Mix_HookMusicFinished(hook_finished);
	sfx_init_al();
}

void sfx_free(void)
{
	sfx_free_al();
	Mix_Quit();
}

void sfx_play(unsigned id)
{
	if (!id || (ge_cfg.options & GE_CFG_NO_SOUND))
		return;

	auto clip = sfx.find(id);

	if (clip == sfx.end())
		sfx_load(id);

	clip = sfx.find(id);
	if (clip != sfx.end())
		clip->second.get()->play();
	else
		fprintf(stderr, "sfx: could not play sound ID %u\n", id);
}

void mus_play(unsigned id)
{
	if (ge_cfg.options & GE_CFG_NO_MUSIC)
		return;

	char buf[FS_BUFSZ];

	const char *name = "";

	if (id >= _ge_state.msc_count)
		fprintf(stderr, "sfx: bad music ID %u\n", id);
	else
		name = ge_start_cfg.msc[id];

	if (fs_cdrom_audio_path(buf, sizeof buf, name)) {
		fprintf(stderr, "sfx: could not load music ID %u\n", id);
		return;
	}

	Mix_Music *mus = Mix_LoadMUS(buf);
	if (!mus) {
		fprintf(stderr, "sfx: could not load music ID %u: %s\n", id, Mix_GetError());
		return;
	}
	music.reset(new ClipMusicFile(id, mus));
	music->play();

	music_playing = 1;
}

}
