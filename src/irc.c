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

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "irc.h"
#include "dcc.h"
#include "main.h"
#include "socket.h"
#include "util.h"

int sfd;
FILE *srv;
char *buf = NULL;
size_t bufsize = 0;
char *prefix, *command, *params, *trail;

static void send_message(char *format, ...)
{
	va_list args;

	va_start(args, format);
	socket_send_message(srv, format, args);
	va_end(args);
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

void do_irc()
{
	struct queue_ent *q = queue_head;
	time_t old_time;

	sfd = socket_connect(host, port);
	srv = fdopen(sfd, "r+");

	/* Login */
	send_message("NICK %s", nick);
	send_message("USER %s * * :%s", nick, nick);
	fflush(srv);

	setbuf(srv, NULL);

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
	while (q) {
		/* Retrieving package number from bot */
		send_message("PRIVMSG %s :@find %s", q->bot, q->title);
		old_time = time(NULL);
		do {
			parse_message();
			if (time(NULL) - old_time > MAX_BOT_WAIT) {
				WARNING("%s did not respond with package list; Skipping entry\n", q->bot);
				q = q->next;
				continue;
			}
		} while (strcmp(prefix, q->bot) != 0 &&
			 strncmp("XDCC SERVER", trail, 11) != 0);

		/* Finding package number */
		trail = skip(trail, '#');
		skip(trail, ':');

		/* Requesting package */
		send_message("PRIVMSG %s :xdcc send %s", q->bot, trail);
		old_time = time(NULL);
		do {
			parse_message();
			if (time(NULL) - old_time > MAX_BOT_WAIT) {
				WARNING("%s did not send DCC request; Skipping entry\n", q->bot);
				q = q->next;
				continue;
			}
		} while (strcmp(prefix, q->bot) != 0 &&
			 strncmp("\001DCC SEND", trail, 9) != 0);

		/* Splitting DCC parameters */
		char *ip, *port, *filesize;

		trail = skip(trail, '"');
		trail = skip(trail, '"');

		ip = ++trail;
		port = skip(ip, ' ');
		filesize = skip(port, ' ');

		LOG("Downloading '%s' from '%s'", q->title, q->bot);
		dcc_do(q->title, ip, port, filesize);

		q = q->next;
	}

	LOG("Download queue completed; Disconnecting");

cleanup:
	send_message("QUIT");
	free(buf);
	fclose(srv);
	close(sfd);
}
