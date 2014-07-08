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
#include <sys/stat.h>
#include <unistd.h>

#include "main.h"
#include "irc.h"
#include "rss.h"
#include "util.h"

char *logfile_name;
char *feeds_name;

static void read_config(char *buf, char **dest)
{
	int len;

	buf = skip(buf, '=');
	trim(buf);
	len = strlen(buf);

	*dest = calloc(len, sizeof(char) + 1);
	if (!*dest) {
		ERROR("Unable to allocate config: Out of Memory; Exiting...\n");
		exit(EXIT_FAILURE);
	}

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
	if (!e) {
		ERROR("Unable to allocate episode structure; Out of Memory; Exiting...\n");
		exit(EXIT_FAILURE);
	}

	read_config(buf, &e->name);

	if (s->head)
		e->next = s->head;
	s->head = e;
}

static struct series_ent* series_init(struct dirent *ent)
{
	struct series_ent *s;
	FILE *file;
	char *filename;
	char *buf = NULL;
	size_t bufsize = 0;

	s = calloc(1, sizeof(*s));
	if (!s) {
		ERROR("Unable to allocate series structure: Out of Memory; Exiting...\n");
		exit(EXIT_FAILURE);
	}

	filename = calloc(256, sizeof(char));
	if (!filename) {
		ERROR("Unable to allocate filename: Out of Memory; Exiting...\n");
		exit(EXIT_FAILURE);
	}

	strcat(filename, feeds_name);
	strcat(filename, "/");
	strcat(filename, ent->d_name);

	s->filename = filename;

	file = fopen(filename, "r");
	if (!file) {
		WARNING("Unable to open '%s'; Ignoring...\n", ent->d_name);
		goto error0;
	}

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
		WARNING("Invalid RSS feed %s; Ignoring...\n", filename);
		goto error1;
	}

	DEBUG("FILE: %s, FEED: %s/%s, BOT:%s\n",
			filename, s->host, s->link, s->bot);

#ifdef ENABLE_DEBUG
	struct episode_ent *ep = s->head;
	
	while (ep) {
		DEBUG("Have: %s\n", ep->name);
		ep = ep->next;
	}
#endif

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

	dir = opendir(feeds_name);
	if (!dir) {
		ERROR("RSS feeds directory not found; Exiting...\n");
		exit(EXIT_FAILURE);
	}

	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;
		struct series_ent *s;

		s = series_init(ent);
		if (!s)
			continue;

		if (series_list)
			s->next = series_list;
		series_list = s;
	}

	closedir(dir);
}

static void main_config_finalize()
{
	free(host);
	free(port);
	free(nick);
}

static void main_config_init()
{
	FILE *file;
	char *buf = NULL;
	size_t bufsize = 0;
	char path[255] = {0};
	
	strcat(path, getenv("HOME"));
	strcat(path, "/.rssdcc");

	file = fopen(path, "r");
	if (!file) {
		fprintf(stderr, "%s not found; Exiting...\n", path);
		exit(EXIT_FAILURE);
	}

	while (getline(&buf, &bufsize, file) > 0){
		if (buf[0] == '#')
			continue;

		if (!strncmp("host", buf, 4))
			read_config(buf, &host);

		if (!strncmp("port", buf, 4))
			read_config(buf, &port);

		if (!strncmp("nick", buf, 4))
			read_config(buf, &nick);

		if (!strncmp("chan", buf, 4))
			read_config(buf, &chan);

		if (!strncmp("log", buf, 3))
			read_config(buf, &logfile_name);

		if (!strncmp("feeds", buf, 5))
			read_config(buf, &feeds_name);

		if (!strncmp("downloads", buf, 9))
			read_config(buf, &downloads_name);
	}

	if (!host || !port || !nick || !logfile_name || !feeds_name) {
		fprintf(stderr, "Invalid config file; Exiting...\n");
		exit(EXIT_FAILURE);
	}

	free(buf);
	fclose(file);
}

int main (int argc, char *argv[])
{
	pid_t pid, sid;

	/* Daemon initialisation*/
	/* Fork off the parent process */
	pid = fork();
	if (pid < 0)
		exit(EXIT_FAILURE);

	/* Exit parent process */
	if (pid > 0)
		exit(EXIT_SUCCESS);

	/* Default umask for Downloads */
	umask(0133);

	main_config_init();

	/* Opening the log */
	logfile = fopen(logfile_name, "w");
	if (!logfile) {
		fprintf(stderr,"Cannot open %s; Exiting...\n", logfile_name);
		exit(EXIT_FAILURE);
	}
	setbuf(logfile, NULL);

	/* Creating SID for child process */
	sid = setsid();
	if (sid < 0) {
		ERROR("Cannot create SID; Exiting...\n");
		exit(EXIT_FAILURE);
	}

	/* Change directory */
	if (chdir("/") < 0) {
		ERROR("Cannot change directory; Exiting...\n");
		exit(EXIT_FAILURE);
	}

	/* Closing standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	series_list = NULL;
	queue_head = NULL;
	queue_tail = NULL;

	DEBUG("IRC HOST: %s\n", host);
	DEBUG("IRC Port: %s\n", port);
	DEBUG("IRC Nick: %s\n", nick);

	while (1) {
		DEBUG("Waking up\n");

		rss_config_init();
		do_rss();

		if (queue_head != NULL) {
			do_irc();
			DEBUG("All tasks finished\n");
		} else {
			DEBUG("Nothing to do\n");
		}
		
		rss_config_finalize();

		/* 5 minutes */
		sleep(300);
	}

	main_config_finalize();
	exit(EXIT_SUCCESS);
}
