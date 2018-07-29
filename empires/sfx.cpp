/* Copyright 2018 the Age of Empires Free Software Remake authors. See LEGAL for legal info */

#include "sfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <map>
#include <memory>

#include "fs.h"
#include "../setup/dbg.h"
#include "../setup/def.h"

#define SFX_CHANNEL_COUNT 20

Mix_Chunk *music_chunk = NULL;
int music_playing = 0;

ALCdevice *alc_dev = NULL;
ALCcontext *alc_ctx = NULL;
ALuint al_src[SFX_CHANNEL_COUNT];

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

std::map<unsigned, std::shared_ptr<Clip>> sfx;

void sfx_init_al(void)
{
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
}

void sfx_free_al(void)
{
	alDeleteSources(SFX_CHANNEL_COUNT, al_src);
	alcDestroyContext(alc_ctx);
	alcCloseDevice(alc_dev);
}

void sfx_load(unsigned id)
{
	char fsbuf[FS_BUFSZ];
	Mix_Chunk *data;

	switch (id) {
	case SFX_BUTTON4:
		fs_help_path(fsbuf, sizeof fsbuf, "button4.wav");
		if (data = Mix_LoadWAV(fsbuf))
			//sfx.emplace(std::make_pair(id, std::shared_ptr<Clip>(new ClipFile(id, nullptr))));
			sfx.emplace(id, std::shared_ptr<Clip>(new ClipFile(id, data)));
		else
			fprintf(stderr, "sfx_load: error loading \"%s\": %s\n", fsbuf, Mix_GetError());
		break;
	}
}

extern "C" {

void sfx_init(void)
{
	char buf[256];

	int flags = MIX_INIT_OGG;
	if ((Mix_Init(flags) & flags) != flags) {
		snprintf(buf, sizeof buf, "SDL2 mixer failed to initialize ogg support: %s\n", Mix_GetError());
		panic(buf);
	}
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) == -1) {
		snprintf(buf, sizeof buf, "SDL2 mixer failed to open audio: %s\n", Mix_GetError());
		panic(buf);
	}
	if (Mix_AllocateChannels(MUSIC_CHANNEL_COUNT) < MUSIC_CHANNEL_COUNT) {
		snprintf(buf, sizeof buf, "SDL2 mixer failed to allocate %d channels: %s\n", MUSIC_CHANNEL_COUNT, Mix_GetError());
		panic(buf);
	}
	sfx_init_al();
}

void sfx_free(void)
{
	sfx_free_al();
	Mix_Quit();
}

void sfx_play(unsigned id)
{
	auto clip = sfx.find(id);

	if (clip == sfx.end())
		sfx_load(id);

	clip = sfx.find(id);
	if (clip != sfx.end())
		clip->second.get()->play();
	else
		fprintf(stderr, "sfx: could not play sound ID %u\n", id);
}

}