//#########################
//Pekka Kana 2
//by Janne Kivilahti from Piste Gamez (2003)
//#########################
#pragma once

#include "engine/PFile.hpp"
#include "engine/platform.hpp"

namespace PSound {

const int CHANNELS = 32;//16;

int init();
int update();
int terminate();

int  load_sfx(PFile::Path path);
int  set_channel(int channel, int panoramic, int volume);
int  play_sfx(int index, int volume, int panoramic, int freq);
int  free_sfx(int index);
void reset_sfx();

int  start_music(PFile::Path path);
void set_musicvolume(int volume);
void set_musicvolume_now(int volume);
void stop_music();

}
