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

#ifndef MAIN_H
#define MAIN_H

struct episode_ent {
	char *name;
	struct episode_ent *next;
};

struct series_ent {
	char *filename;

	char *host;
	char *link;
	char *bot;

	/* linked list with all the episodes we have downloaded before */
	struct episode_ent *head;

	struct series_ent *next;
};

/* Holds all of series we're watching and their configs */
struct series_ent *series_list;

struct queue_ent {
	char *title;
	struct series_ent *series;

	struct queue_ent *next;
};

/* Download queue for IRC */
struct queue_ent *queue_head;
struct queue_ent *queue_tail;

#endif
