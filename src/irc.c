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
#include <unistd.h>

#include "irc.h"
#include "dcc.h"
#include "main.h"
#include "socket.h"
#include "util.h"

int sfd;
FILE *srv;

static void send_message(char *format, ...)
{
	va_list args;

	va_start(args, format);
	socket_send_message(srv, format, args);
	va_end(args);
}

static bool parse_message(struct queue_ent *q)
{
	char *buf = NULL;
	size_t bufsize = 0;
	char *prefix = NULL, *command, *params, *trail;

	if (getline(&buf, &bufsize, srv) <= 0) {
		free(buf);
		return true;
	}

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

	if (!strcmp("PING", command))
		send_message("PONG %s", trail?trail:params);

	if (!strncmp(prefix, q->bot, strlen(q->bot))) {
		printf(":%s %s %s : %s", prefix, command, params, trail);

		if (!strcmp("NOTICE", command) && !strncmp("XDCC SERVER", trail, 11)) {
			trail = skip(trail, '#');
			skip(trail, ':');

			send_message("PRIVMSG %s :xdcc send %s", q->bot, trail);
			return false;
		}

		/* DCC message from bot has '1' at start, so trail must be incremented to skip it */
		if (!strncmp("DCC SEND", ++trail, 8)) {
			char *ip, *port, *filesize;

			trail = skip(trail, '"');
			trail = skip(trail, '"');

			ip = ++trail;
			port = skip(ip, ' ');
			filesize = skip(port, ' ');

			printf("Downloading '%s'", q->title);
			dcc_do(q->title, ip, port, filesize);
			return true;
		}
	}

	free(buf);
	return false;
}

static void queue_do()
{
	struct queue_ent *q = queue_head;

	while(q) {
		send_message("PRIVMSG %s :@find %s", q->bot, q->title);

		while(!parse_message(q));

		q = q->next;
	}
}

void do_irc()
{
	sfd = socket_connect(host, port);
	srv = fdopen(sfd, "r+");

	/* Login */
	send_message("NICK %s", nick);
	send_message("USER %s * * :%s", nick, nick);
	fflush(srv);

	setbuf(srv, NULL);

	queue_do();

	send_message("QUIT");
	fclose(srv);
	close(sfd);
}
