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
#include <unistd.h>

#include "rss.h"
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

static void queue_add(struct series_ent *sp, char *title)
{
	struct queue_ent *q;

	q = calloc(1, sizeof(*q));

	q->series = sp;
	q->title = title;

	if (!queue_head) {
		queue_head = q;
		queue_tail = q;
	} else {
		queue_tail->next = q;
		queue_tail = q;
	}
}

static bool config_check_episode(struct series_ent *sp, char *buf)
{
	struct episode_ent *ep = sp->head;
	while (ep && strcmp(buf, ep->name) != 0)
		ep = ep->next;

	return ep != NULL;
}

static void replace_xml_escapes(char *buf)
{
	char *start = buf;
	char *ptr;
	char new[strlen(buf) + 1];
	char esc[2];

	new[0] = '\0';

	while ((ptr = skip(start, '&')) && *ptr != '\0') {
		strcat(new, start);
		start = skip(ptr, ';');

		esc[0] = (char)strtol(++ptr, NULL, 10); 
		esc[1] = '\0';

		strcat(new, esc);
	}

	strcat(new, start);

	memcpy(buf, new, strlen(new) + 1);
}

static void parse_rss(struct series_ent *sp, char *buf)
{
	char *ptr;
	char *title;

	while ((buf = strstr(buf, "<item>")) != NULL && (buf = strstr(buf, "<title>")) != NULL) {
		buf += 7;
		ptr = skip(buf, '<');

		replace_xml_escapes(buf);

		if (config_check_episode(sp, buf)) {
			printf("  Already have: %s\n", buf);
		} else {
			printf("  Queueing: %s\n", buf);
			
			title = calloc(1, strlen(buf) + 1);
			strcpy(title, buf);

			queue_add(sp, title);
		}

		buf = ptr;
	}
}


void do_rss()
{
	struct series_ent *sp = series_list;
	char *buf = NULL;
	size_t bufsize = 0;

	while (sp) {
		sfd = socket_connect(sp->host, "80");
		srv = fdopen(sfd, "r+");

		send_message("GET /%s HTTP/1.1", sp->link);
		send_message("Host: %s", sp->host);
		send_message("Connection: close\r\n");
		fflush(srv);

		while (getline(&buf, &bufsize, srv) > 0)
			parse_rss(sp, buf);

		fclose(srv);
		close(sfd);
		sp = sp->next;
	}

	free(buf);
}
