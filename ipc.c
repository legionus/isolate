// SPDX-License-Identifier: GPL-2.0-or-later
#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>
#include <errno.h>
#include <err.h>

#include "isolate.h"

const char *cmd2str(cmd_t cmd)
{
	switch (cmd) {
		case CMD_INVALID:	return "CMD_INVALID";
		case CMD_FORK:		return "CMD_FORK";
		case CMD_PID:		return "CMD_PID";
		case CMD_EXEC:		return "CMD_EXEC";
	}
	return "UNKNOWN";
}

int ipc_pair(int sv[2])
{
	return socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);
}

int send_cmd(int fd, cmd_t cmd)
{
	if (TEMP_FAILURE_RETRY(write(fd, &cmd, sizeof(cmd))) < 0) {
		warn("send_cmd(%s)", cmd2str(cmd));
		return -1;
	}

	return 0;
}

cmd_t recv_cmd(int fd)
{
	cmd_t cmd;

	if (TEMP_FAILURE_RETRY(read(fd, &cmd, sizeof(cmd))) < 0) {
		warn("recv_cmd(%s)", cmd2str(cmd));
		return CMD_INVALID;
	}

	return cmd;
}

