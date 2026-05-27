#ifndef RINGTONE_H
#define RINGTONE_H

#include <avr/pgmspace.h>
#include "phone.h"

#define RINGTONES_NO 6

extern const uint16_t classic[] PROGMEM;
extern const uint16_t mozart[] PROGMEM;
extern const uint16_t mission_impossible[] PROGMEM;
extern const uint16_t lucky_discovery[] PROGMEM;
extern const uint16_t nokia_tune[] PROGMEM;
extern const uint16_t jingle_bells[] PROGMEM;

extern const uint16_t* const ringtones[] PROGMEM;
extern const uint8_t len_ringtones[] PROGMEM;

void reset_ringtone();

void play_ringtone();

#endif
