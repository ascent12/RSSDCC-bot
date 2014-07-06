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

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "irc.h"
#include "main.h"
#include "socket.h"
#include "util.h"

#define MAX_NUM_THREADS 2

int sfd;
FILE *srv;
char *buf = NULL;
size_t bufsize = 0;
char *prefix, *command, *params, *trail;
pthread_t thread[MAX_NUM_THREADS];
int open_threads = 0;

struct dcc_ent *list = NULL;

static void send_message(char *format, ...)
{
	va_list args;

	va_start(args, format);
	socket_send_message(srv, format, args);
	va_end(args);
}

static void config_add_episode(struct series_ent *sp, char *title)
{
	struct episode_ent *e;
	FILE *file;

	e = calloc(1, sizeof(*e));

	e->name = title;

	if (sp->head)
		e->next = sp->head;
	sp->head = e;

	file = fopen(sp->filename, "a");
	fprintf(file, "have=%s\n", title);
	fclose(file);
}

static void parse_message()
{
	prefix = NULL;

	/* No message */
	if (getline(&buf, &bufsize, srv) <= 0)
		return;

	/* Finding the prefix and command start */
	if (buf[0] == ':') {
		prefix = &buf[1];
		command = skip(buf, ' ');
	} else {
		command = &buf[0];
	}

	/* Finding trail */
	trail = strstr(command, " :");
	if (trail != NULL) {
		*trail = '\0';
		trail += 2;
		trim(trail);
	}

	/* Splitting command and params */
	params = skip(command, ' ');

	/* Automatic PONG, as long as we're parsing */
	if (!strcmp("PING", command))
		send_message("PONG %s", trail?trail:params);
}


static void send_ack(int sfd, int pos)
{
	uint32_t net = htonl(pos & 0xFFFFFFFF);
	send(sfd, (char*)&net, 4, 0);
}

static void draw_progress()
{
	struct dcc_ent *dp = list;

	printf("\033[F");

	if (dp != NULL) {
		printf("\r%.30s... [%dKiB/%dKiB] %.1f%%", dp->q->title,
				dp->position>>10, dp->filesize>>10,
				(float)dp->position / (float)dp->filesize * 100);

		dp = dp->next;
		if (dp != NULL) {
			printf("\n%.30s... [%dKiB/%dKiB] %.1f%%", dp->q->title,
					dp->position>>10, dp->filesize>>10,
					(float)dp->position / (float)dp->filesize * 100);	
		}
	}
}

static void *dcc_do(void *data)
{
	struct dcc_ent *dp = data;
	int sfd, n;
	char dcc_buf[4096];
	bool need_ack = false;

	sfd = socket_connect(dp->ip, dp->port);

	/* Main DCC loop */
	while (1) {
		n = recv(sfd, dcc_buf, sizeof(dcc_buf), 0);

		if (n < 1) {
			if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					if (need_ack)
						send_ack(sfd, dp->position);
			} else {
				ERROR("Unexpected DCC failure; Closing connection\n");
				break;
			}
		}

		if (write(dp->fd, dcc_buf, n) < 0) {
			ERROR("Unable to write DCC to file; Closing connection\n");
			if (need_ack)
				send_ack(sfd, dp->position);
			break;
		}

		dp->position += n;
		need_ack = true;

		if (dp->position >= dp->filesize) {
			send_ack(sfd, dp->position);
			LOG("\r");
			LOG("Sucessfully downloaded '%s'\n", dp->q->title);
			config_add_episode(dp->q->series, dp->q->title);
			break;
		}
	}

	/* Cleanup */
	list = dp->next;
	close(sfd);
	close(dp->fd);
	free(dp->ip);
	free(dp->port);
	free(dp);

	/* Thread is closed here */
	--open_threads;
	return 0;
}

static bool dcc_create(struct queue_ent *q, char *dcc, int i)
{
	struct dcc_ent *dp;
	struct stat stat;
	char *ptr = dcc;
	char pos_str[20];
	time_t old_time;

	dp = calloc(1, sizeof(*dp));

	ptr = skip(dcc, ' ');
	dp->ip = calloc(1, strlen(dcc) + 1);
	strcpy(dp->ip, dcc);
	dcc = ptr;

	ptr = skip(dcc, ' ');
	dp->port = calloc(1, strlen(dcc) + 1);
	strcpy(dp->port, dcc);
	
	dp->filesize = (int)strtol(ptr, NULL, 10);
	dp->q = q;

	if ((dp->fd = open(q->title, O_CREAT | O_WRONLY)) < 0) {
		ERROR("Could not open '%s' for writing\n", q->title);
		goto cleanup;
	}

	/* Checking too see if file is empty or not */
	if (!fstat(dp->fd, &stat)) {
		if ((dp->position = stat.st_size) != 0) {
			/* Using DCC resume */
			if (dp->position >= dp->filesize) {
				LOG("Found completed '%s'; Skipping", q->title);
				return true;
			}

			snprintf(pos_str, 20, "%d", dp->position);

			send_message("PRIVMSG %s :\001DCC RESUME \"%s\" %s %s\001",
					q->series->bot, q->title, dp->port, pos_str);
			old_time = time(NULL);
			do {
				parse_message();
				if (time(NULL) - old_time > MAX_BOT_WAIT) {
					LOG("DCC ACCEPT not recived; Continuing from start\n");
					dp->position = 0;
					goto out;
				}
			} while (strcmp(prefix, q->series->bot) != 0 &&
				 strncmp("\001DCC ACCEPT", trail, 11) != 0);

			LOG("Resuming '%s'\n", q->title);
			lseek(dp->fd, 0, SEEK_END);
		}
	} else {
		WARNING("Could not stat '%s'; Assuming it's empty\n", q->title);
	}

out:
	dp->next = list;
	list = dp;

	++open_threads;
	pthread_create(&thread[i], NULL, dcc_do, (void*)dp);

	return true;

cleanup:
	free(dp->ip);
	free(dp->port);
	free(dp);
	return false;
}

void do_irc()
{
	struct queue_ent *q;
	time_t old_time;
	bool timeout;
	int i = 0;

	sfd = socket_connect(host, port);
	srv = fdopen(sfd, "r+");

	/* Login */
	send_message("NICK %s", nick);
	send_message("USER %s * * :%s", nick, nick);
	fflush(srv);

	setbuf(srv, NULL);
//	setbuf(stdout, NULL);

	/* Waiting for 001 RPL_WELCOME */
	old_time = time(NULL);
	do {
		parse_message();
		if (time(NULL) - old_time > MAX_WELCOME_WAIT) {
			ERROR("%s did not return 001 RPL_WELCOME; Disconnecting\n", host);
			goto cleanup;
		}
	} while (strcmp("001", command) != 0);

	/* Waiting for 376 RPL_ENDOFMOTD */
	old_time = time(NULL);
	do {
		parse_message();
		if (time(NULL) - old_time > MAX_WELCOME_WAIT) {
			ERROR("%s did not finish MOTD; Disconnecting\n", host);
			goto cleanup;
		}
	} while (strcmp("376", command) != 0);

	/* Main IRC loop */
	for (q = queue_head; q; q = q->next, timeout = false) {

		/* Retrieving package number from bot */
		send_message("PRIVMSG %s :@find %s", q->series->bot, q->title);
		old_time = time(NULL);
		do {
			parse_message();
			if (time(NULL) - old_time > MAX_BOT_WAIT) {
				WARNING("%s did not respond with package list; Skipping entry\n", q->series->bot);
				timeout = true;
				break;
			}
		} while (strcmp(prefix, q->series->bot) != 0 &&
			 strncmp("XDCC SERVER", trail, 11) != 0);

		if (timeout)
			continue;

		/* Finding package number */
		trail = skip(trail, '#');
		skip(trail, ':');

		/* Requesting package */
		send_message("PRIVMSG %s :xdcc send %s", q->series->bot, trail);
		old_time = time(NULL);
		do {
			parse_message();
			if (time(NULL) - old_time > MAX_BOT_WAIT) {
				WARNING("%s did not send DCC request; Skipping entry\n", q->series->bot);
				timeout = true;
				break;
			}
		} while (strcmp(prefix, q->series->bot) != 0 &&
			 strncmp("\001DCC SEND", trail, 9) != 0);

		if (timeout)
			continue;

		/* Splitting DCC parameters */

		trail = skip(trail, '"');
		trail = skip(trail, '"');

		LOG("Downloading '%s' from '%s'\n", q->title, q->series->bot);
		dcc_create(q, ++trail, i++);

		/* Block when max threads are open */
		while (open_threads >= MAX_NUM_THREADS) {
			draw_progress();
//			parse_message();
		}
	}

	/* Block if everything hasn't completed */
	while (open_threads != 0) {
		draw_progress();
//		parse_message();
	}

	LOG("Download queue completed; Disconnecting\n");

cleanup:
	send_message("QUIT");
	free(buf);
	fclose(srv);
	close(sfd);
}
