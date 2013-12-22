/*
 * Nestopia UE
 * 
 * Copyright (C) 2012-2013 R. Danbrook
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 */

#include <stdio.h>

#include <SDL.h>

#include "core/api/NstApiEmulator.hpp"
#include "core/api/NstApiSound.hpp"

#include "config.h"
#include "newaudio.h"

using namespace Nes::Api;

extern settings conf;
extern Emulator emulator;

void audio_init() {
	printf("Init audio\n");
	Sound sound(emulator);
	
	sound.SetSampleBits(16);
	sound.SetSampleRate(conf.audio_sample_rate);
	sound.SetVolume(Sound::ALL_CHANNELS, conf.audio_volume);
	
	sound.SetSpeaker(Sound::SPEAKER_MONO);
	//sound.SetSpeed(60);
}
