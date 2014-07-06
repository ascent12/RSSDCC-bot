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
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dcc.h"
#include "socket.h"
#include "util.h"

static void send_ack(int sfd, int pos)
{
	uint32_t net = htonl(pos & 0xFFFFFFFF);
	send(sfd, (char*)&net, 4, 0);
}

bool dcc_do(char *title, char *ip, char *port, char *filesize)
{
	int sfd;
	int ofd;
	int n, size, pos = 0;
	char buf[4096];
	bool need_ack = false, ret;

	size = (int)strtol(filesize, NULL, 10);

	ofd = open(title, O_CREAT | O_WRONLY);
	if (ofd < 0) {
		ERROR("Could not open '%s' for writing\n", title);
		return false;
	}

	sfd = socket_connect(ip, port);

	while (1) {
		n = recv(sfd, buf, sizeof(buf), 0);

		printf("\r[%dK/%dK]", pos>>10, size>>10);

		if (n < 1) {
			if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK)
					if (need_ack)
						send_ack(sfd, pos);
			} else {
				ERROR("Unexpected DCC failure; Closing connection\n");
				ret = false;
				goto close;
			}
		}

		if (write(ofd, buf, n) < 0) {
			ERROR("Unable to write DCC to file; Closing connection\n");
			if (need_ack)
				send_ack(sfd, pos);
			ret = false;
			goto close;
		}

		pos += n;
		need_ack = true;

		if (pos >= size) {
			send_ack(sfd, pos);
			LOG("\r");
			LOG("Sucessfully downloaded %s file\n", title);
			ret = true;
			goto close;
		}
	}

close:
	close(sfd);
	close(ofd);
	return ret;
}


