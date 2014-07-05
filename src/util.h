/* This file is part of RSSDCC-bot.
 *
 * Copyright (C) 2014 Scott Anderson.
 *
 * Foobar is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BOT_UTIL_H
#define BOT_UTIL_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MESSAGE(type, format, ...) \
	fprintf(stderr, "[" type "]: " format, ## __VA_ARGS__);

#define WARNING(format, ...)	MESSAGE("WARNING", format, ## __VA_ARGS__)
#define ERROR(format, ...)	MESSAGE("ERROR", format, ## __VA_ARGS__)

char *skip(char *s, char c);

void trim(char *s);

#endif
