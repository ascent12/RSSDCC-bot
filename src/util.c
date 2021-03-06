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
 * along with RSSDCC-bot.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <string.h>

#include "util.h"

char *skip(char *s, char c)
{
	while (*s != c && *s != '\0')
		++s;
	if (*s != '\0')
		*s++ = '\0';
	return s;
}

void trim(char *s)
{
	char *e;

	e = s + strlen(s) - 1;
	while (isspace(*e) && e > s)
		--e;
	*(e + 1) = '\0';
}
