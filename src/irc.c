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
#include <semaphore.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
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

/* Set empty strings to this, instead of NULL, to prevent errors */
char nul = '\0';

pthread_t thread[MAX_NUM_THREADS], ping_thread;
pthread_attr_t dcc_attr;
sem_t dcc_download;

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

static void send_ack(int sfd, int pos)
{
	uint32_t net = htonl(pos & 0xFFFFFFFF);
	send(sfd, (char*)&net, 4, 0);
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
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					if (need_ack) {
						send_ack(sfd, dp->position);
						need_ack = false;
					}

					continue;
				}
			} else {
				WARNING("Unexpected DCC failure for %s: %s; Closing connection\n",
						dp->q->title, strerror(errno));
				break;
			}
		}

		if (write(dp->fd, dcc_buf, n) < 0) {
			WARNING("Unable to write DCC to file for %s: %s; Closing connection\n",
					dp->q->title, strerror(errno));
			if (need_ack)
				send_ack(sfd, dp->position);
			break;
		}

		dp->position += n;
		need_ack = true;

		if (dp->position >= dp->filesize) {
			send_ack(sfd, dp->position);
			LOG("Sucessfully downloaded %s\n", dp->q->title);
			config_add_episode(dp->q->series, dp->q->title);
			break;
		}
	}

	/* Cleanup */
	close(sfd);
	close(dp->fd);
	free(dp->ip);
	free(dp->port);
	free(dp);

	/* Thread is closed here */
	sem_post(&dcc_download);
	return 0;
}

static void parse_message()
{
	prefix = &nul;

	if (getline(&buf, &bufsize, srv) <= 0)
		/* No message */
		return;

	DEBUG("%s", buf);

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
	} else {
		trail = &nul;
	}

	/* Splitting command and params */
	params = skip(command, ' ');

	/* Automatic PONG, as long as we're parsing */
	if (!strcmp("PING", command))
		send_message("PONG %s", trail?trail:params);
}

static void *ping()
{
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	while (1) {
		parse_message();
		/* To stop it from going too crazy */
		sleep(1);
	}
	return 0;
}

static bool wait_for_command(char *str, int max_wait)
{
	time_t old_time = time(NULL);
	int len = strlen(str);

	do {
		parse_message();
		if (time(NULL) - old_time > max_wait)
			return false;
	} while (strncmp(str, command, len) != 0);

	return true;
}

static bool wait_for_trail(char *str, char *bot, int max_wait)
{
	time_t old_time = time(NULL);
	int len = strlen(str);

	do {
		parse_message();
		if (time(NULL) - old_time > max_wait)
			return false;
	} while (strcmp(prefix, bot) != 0 &&
		 strncmp(str, trail, len) != 0);

	return true;
}

static void dcc_create(struct queue_ent *q, char *dcc, int i)
{
	struct dcc_ent *d;
	struct stat stat;
	char *ptr = dcc;
	char pos_str[20];
	char filename[256] = {0};
	off_t off;

	d = calloc(1, sizeof(*d));
	if (!d) {
		ERROR("Unable to allocate DCC structure: Out of Memory; Exiting...\n");
		exit(EXIT_FAILURE);
	}

	/* Getting IP address of DCC bot */
	ptr = skip(dcc, ' ');
	d->ip = calloc(1, strlen(dcc) + 1);
	if (!d->ip) {
		ERROR("Unable to allocate ip string: Out of Memory; Exiting...\n");
		exit(EXIT_FAILURE);
	}
	strcpy(d->ip, dcc);
	dcc = ptr;

	/* Getting port of the DCC bot */
	ptr = skip(dcc, ' ');
	d->port = calloc(1, strlen(dcc) + 1);
	if (!d->port) {
		ERROR("Unable to allocate port string: Out of Memory; Exiting...\n");
		exit(EXIT_FAILURE);
	}
	strcpy(d->port, dcc);
	
	d->filesize = (int)strtol(ptr, NULL, 10);
	d->q = q;

	strcat(filename, downloads_name);
	strcat(filename, "/");
	strcat(filename, q->title);

	d->fd = open(filename, O_CREAT | O_WRONLY, 0644);
	if (d->fd < 0) {
		WARNING("Could not open %s for writing; Skipping entry...\n", q->title);
		free(d->ip);
		free(d->port);
		free(d);
		sem_post(&dcc_download);
		return;
	}

	/* Checking too see if file is empty or not */
	if (!fstat(d->fd, &stat) && (d->position = stat.st_size) != 0) {
		/* Using DCC resume */
		DEBUG("Existing file found; Attempting DCC resume...\n");

		if (d->position >= d->filesize) {
			LOG("Found completed %s; Skipping\n", q->title);
			config_add_episode(q->series, q->title);
			sem_post(&dcc_download);
			return;
		}

		snprintf(pos_str, 20, "%d", d->position);

		send_message("PRIVMSG %s :\001DCC RESUME \"%s\" %s %s\001",
				q->series->bot, q->title, d->port, pos_str);
		if(wait_for_trail("\001DCC ACCEPT", q->series->bot, MAX_BOT_WAIT)) {
			LOG("Resuming %s\n", q->title);

			off = lseek(d->fd, d->position, SEEK_SET);
			if (off != d->position) {
				WARNING("Cannot seek file; Continuing from start...\n");
				d->position = 0;
			}
		} else {
			LOG("DCC ACCEPT not recived; Continuing from start\n");
			d->position = 0;
		}
	}

	pthread_create(&thread[i], &dcc_attr, dcc_do, (void*)d);
}

void do_irc()
{
	struct queue_ent *q, *tmp;
	int i = 0;

	DEBUG("Connecting to %s:%s\n", host, port);

	/* Opening socket to IRC channel */
	sfd = socket_connect(host, port);
	if (sfd < 0) {
		WARNING("Unable to open socket for %s:%s; Download queue failed...\n",
				host, port);
		goto error0;
	}

	/* Opening the socket as a file */
	srv = fdopen(sfd, "r+");
	if (!srv) {
		WARNING("Unable to open socket as file for %s:%s; Download queue failed...\n",
				host, port);
		goto error1;
	}

	/* Creating a semaphore to limit amount of concurrent downloads */
	if (sem_init(&dcc_download, 0, MAX_NUM_THREADS) < 0) {
		WARNING("Unable to open semaphore; Download queue failed...\n");
		goto error1;
	}

	/* Setting pthread attributes, which are used later */
	pthread_attr_init(&dcc_attr);
	pthread_attr_setdetachstate(&dcc_attr, PTHREAD_CREATE_DETACHED);

	/* Login */
	send_message("NICK %s", nick);
	send_message("USER %s * * :%s", nick, nick);
	fflush(srv);

	setbuf(srv, NULL);

	/* Waiting for 001 RPL_WELCOME */
	if (!wait_for_command("001", MAX_WELCOME_WAIT)) {
		WARNING("%s did not return 001 RPL_WELCOME; Disconnecting\n", host);
		goto error2;
	}
	DEBUG("001 RPL_WELCOME recieved\n");

	/* Waiting for 376 RPL_ENDOFMOTD */
	if (!wait_for_command("376", MAX_WELCOME_WAIT)) {
		WARNING("%s did not finish MOTD; Disconnecting\n", host);
		goto error2;
	}
	DEBUG("376 RPL_ENDOFMOTD recieved\n");

	/* Using registered nick */
	send_message("PRIVMSG NickServ :IDENTIFY %s", pass);

	/* Joining channel and waiting for 332 RPL_TOPIC */
	send_message("JOIN #%s", chan);
	if (!wait_for_command("332", MAX_WELCOME_WAIT)) {
		WARNING("Could not join #%s; Continuing...\n", chan);
		WARNING("  Note: This may cause your download request to be rejected\n");
	} else {
		DEBUG("Joined #%s\n", chan);
	}

	/* Main IRC loop */
	for (q = queue_head; q; q = q->next) {
		/* Create a seperate thread to handle pinging,
		 * as the main thread may get blocked */
		pthread_create(&ping_thread, NULL, ping, NULL);

#ifdef ENABLE_DEBUG
		int semv;
		sem_getvalue(&dcc_download, &semv);
		DEBUG("Current semaphore value is %d\n", semv);
		DEBUG("Trying to grab semaphore\n");
#endif

		/* Block when max threads are open */
		sem_wait(&dcc_download);

		/* Close the pinging thread, as it will screw with
		 * us trying to negotiate a DCC connection */
		pthread_cancel(ping_thread);

		DEBUG("Past blocking point\n");

		/* Retrieving package number from bot */
		send_message("PRIVMSG %s :@find %s", q->series->bot, q->title);
		if (!wait_for_trail("XDCC SERVER", q->series->bot, MAX_BOT_WAIT)) {
			WARNING("%s did not respond with package list; Skipping entry\n", q->series->bot);
			sem_post(&dcc_download);
			continue;
		}

		/* Finding package number */
		trail = skip(trail, '#');
		skip(trail, ':');

		/* Requesting package */
		send_message("PRIVMSG %s :xdcc send %s", q->series->bot, trail);
		if (!wait_for_trail("\001DCC SEND", q->series->bot, MAX_BOT_WAIT)) {
			WARNING("%s did not send DCC request; Skipping entry\n", q->series->bot);
			sem_post(&dcc_download);
			continue;
		}

		/* Splitting DCC parameters */
		trail = skip(trail, '"');
		trail = skip(trail, '"');

		LOG("Downloading %s from %s\n", q->title, q->series->bot);
		dcc_create(q, ++trail, i++);
	}

	pthread_create(&ping_thread, NULL, ping, NULL);

	/* Block if everything hasn't completed */
	int semv;
	do
		sem_getvalue(&dcc_download, &semv);
	while (semv != MAX_NUM_THREADS);

	pthread_cancel(ping_thread);

	LOG("Download queue completed; Disconnecting\n");

error2:
	send_message("QUIT");
	free(buf);
	bufsize = 0;
	sem_destroy(&dcc_download);
	pthread_attr_destroy(&dcc_attr);

error1:
	fclose(srv);
error0:
	close(sfd);

	q = queue_head;
	while (q) {
		tmp = q;
		q = q->next;
		free(tmp);
	}
	queue_head = NULL;

}
