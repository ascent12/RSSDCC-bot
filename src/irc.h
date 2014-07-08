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

#ifndef IRC_H
#define IRC_H

#define MAX_WELCOME_WAIT 10
#define MAX_BOT_WAIT 10

struct dcc_ent {
	int fd;

	char *ip;
	char *port;

	int filesize;
	int position;

	struct queue_ent *q;
	struct dcc_ent *next;
};

char *host, *port, *nick, *chan, *downloads_name;

void do_irc();

#endif
