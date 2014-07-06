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

#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "main.h"
#include "irc.h"
#include "rss.h"
#include "util.h"

static void read_config(char *buf, char **dest)
{
	int len;
	buf = skip(buf, '=');
	trim(buf);
	len = strlen(buf);
	*dest = calloc(len, sizeof(char) + 1);
	strncpy(*dest, buf, len);
}

static void rss_config_finalize()
{
	struct series_ent *sp;
	struct episode_ent *ep;

	while (series_list != NULL) {
		sp = series_list;
		series_list = series_list->next;

		free(sp->filename);
		free(sp->host);
		free(sp->link);
		free(sp->bot);
		
		while (sp->head != NULL) {
			ep = sp->head;
			sp->head = sp->head->next;

			free(ep->name);
			free(ep);
		}

		free(sp);
	}
}

static void episode_init(char *buf, struct series_ent *s)
{
	struct episode_ent *e;

	e = calloc(1, sizeof(*e));

	read_config(buf, &e->name);

	if (s->head)
		e->next = s->head;
	s->head = e;
}

static struct series_ent* series_init(struct dirent *ent, int id)
{
	struct series_ent *s;
	FILE *file;
	char *filename;
	char *buf = NULL;
	size_t bufsize = 0;

	s = calloc(1, sizeof(*s));

	filename = calloc(64, sizeof(char));

	strcat(filename, "feeds/");
	strcat(filename, ent->d_name);

	file = fopen(filename, "r");
	if (!file) {
		WARNING("Unable to open \"%s\"\n", ent->d_name);
		goto error0;
	}

	s->id = id;
	s->filename = filename;

	while (getline(&buf, &bufsize, file) > 0) {
		if (buf[0] == '#')
			continue;

		if (!strncmp("host", buf, 4))
			read_config(buf, &s->host);

		if (!strncmp("link", buf, 4))
			read_config(buf, &s->link);

		if (!strncmp("bot", buf, 3))
			read_config(buf, &s->bot);

		if (!strncmp("have", buf, 4))
			episode_init(buf, s); 
	}

	if (!s->host || !s->link || !s->bot) {
		WARNING("Invalid config file %s\n", filename);
		goto error1;
	}

	fclose(file);
	free(buf);
	return s;

error1:
	free(buf);
	if (s->host)
		free(s->host);
	if (s->link)
		free(s->link);
	if (s->bot)
		free(s->bot);
error0:
	free(filename);
	free(s);
	return NULL;
}

static void rss_config_init()
{
	DIR *dir;
	struct dirent *ent;
	int id = 0;

	if ((dir = opendir("feeds")) != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			if (ent->d_name[0] == '.')
				continue;
			struct series_ent *s;

			s = series_init(ent, id++);
			if (!s)
				continue;

			if (series_list)
				s->next = series_list;
			series_list = s;
		}

		closedir(dir);
		return;
	}

	ERROR("No RSS feeds found. Exiting...\n");
	exit(EXIT_FAILURE);
}

static void irc_config_finalize()
{
	free(host);
	free(port);
	free(nick);
}

static void irc_config_init()
{
	FILE *file;
	char *buf = NULL;
	size_t bufsize = 0;

	file = fopen("rssdcc.conf", "r");
	if (file) {
		while (getline(&buf, &bufsize, file) > 0){
			if (buf[0] == '#')
				continue;

			if (!strncmp("host", buf, 4))
					read_config(buf, &host);

			if (!strncmp("port", buf, 4))
					read_config(buf, &port);

			if (!strncmp("nick", buf, 4))
					read_config(buf, &nick);
		}

		free(buf);
		fclose(file);
	} else {
		ERROR("Configuration file not found. Exiting...\n");
		exit(EXIT_FAILURE);
	}
}

int main (int argc, char *argv[])
{
	series_list = NULL;
	queue_head = NULL;
	queue_tail = NULL;

	irc_config_init();
	rss_config_init();

	printf("IRC CONFIG:\n  Host: %s\n  Port: %s\n  Nick: %s\n\n",
			host, port, nick);

	struct series_ent *sp;
	struct episode_ent *ep;
	if (series_list) {
		printf("RSS CONFIG:");
		for (sp = series_list; sp != NULL; sp = sp->next) {
			printf("\n%s\n  ID: %d\n  Host: %s\n  Link: %s\n  Bot: %s\n",
					sp->filename, sp->id, sp->host, sp->link, sp->bot);
			for (ep = sp->head; ep != NULL; ep = ep->next)
				printf("    Already have: %s\n", ep->name);
		}
		printf("\n\n");
	}

	do_rss();
	if (queue_head != NULL)
		do_irc();

	LOG("All tasks finished! Exiting...\n");

	rss_config_finalize();
	irc_config_finalize();

	return 0;
}
