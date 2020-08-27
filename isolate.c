#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <sched.h> // CLONE_*
#include <sysexits.h>
#include <errno.h>
#include <err.h>

#include "isolate.h"

#ifndef __progname
char *__progname;
#endif

int verbose = 0;
int unshare_flags = 0;
int uid = -1;
int gid = -1;
char *rootdir = NULL;

static void usage(int code)
{
	fprintf(stdout,
	        "Usage: isolate [options] [--] <command> [<arguments>]\n"
	        "\n"
	        "Utility allows to isolate process inside predefined environment.\n"
	        "\n"
	        "Options:\n"
	        " -R, --root=DIR        run the command with root directory set to DIR\n"
	        " -U, --unshare=LIST    list of namespaces that must be unshared\n"
	        " -u, --user=UID        set uid in entered namespace\n"
	        " -g, --group=GID       set gid in entered namespace\n"
	        " -h, --help            display this help and exit\n"
	        " -v, --verbose         print a message for each action\n"
	        " -V, --version         output version information and exit\n"
	        "\n"
	        "Report bugs to authors.\n"
	        "\n");
	exit(code);
}

static void print_version_and_exit(void)
{
	fprintf(stdout, "isolate version %s\n", VERSION);
	fprintf(stdout,
	        "Written by Alexey Gladkov.\n\n"
	        "Copyright (C) 2020  Alexey Gladkov <gladkov.alexey@gmail.com>\n"
	        "This is free software; see the source for copying conditions.  There is NO\n"
	        "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
	exit(EXIT_SUCCESS);
}

static int parse_arguments(int argc, char **argv)
{
	const char short_opts[] = "vVhU:u:g:R:";
	const struct option long_opts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ "unshare", required_argument, NULL, 'U' },
		{ "user", required_argument, NULL, 'u' },
		{ "group", required_argument, NULL, 'g' },
		{ "root", required_argument, NULL, 'R' },
		{ NULL, 0, NULL, 0 }
	};
	int c;

	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != EOF) {
		switch (c) {
			case 'R':
				rootdir = optarg;
				break;
			case 'U':
				if (parse_unshare_namespaces(&unshare_flags, optarg) < 0)
					return -1;
				break;
			case 'u':
				uid = atoi(optarg);
				break;
			case 'g':
				gid = atoi(optarg);
				break;
			case 'h':
				usage(EXIT_SUCCESS);
				break;
			case 'v':
				verbose++;
				break;
			case 'V':
				print_version_and_exit();
				break;
		}
	}

	if (optind == argc) {
		warnx("more arguments required");
		return -1;
	}

	return 0;
}

static int main_parent(int sock, pid_t helper_pid)
{
	pid_t pid;
	cmd_t cmd;
	int status;

	__progname = (char *) "isolate: parent";

	if (prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0) < 0)
		err(EX_OSERR, "prctl(PR_SET_CHILD_SUBREAPER)");

	if (send_cmd(sock, CMD_FORK) < 0)
		errx(EX_IOERR, NULL);

	if ((cmd = recv_cmd(sock)) != CMD_PID)
		errx(EX_IOERR, "unexpected message: %s", cmd2str(cmd));

	if (TEMP_FAILURE_RETRY(read(sock, &pid, sizeof(pid))) < 0)
		err(EX_IOERR, "read(pid)");

	if (verbose > 1)
		warnx("container created (pid=%d)", pid);

	if (waitpid(helper_pid, &status, 0) < 0)
		err(EX_SOFTWARE, "waitpid");

	if (WEXITSTATUS(status) != EXIT_SUCCESS)
		return WEXITSTATUS(status);

	if ((unshare_flags & CLONE_NEWUSER) && (uid >= 0 || gid >= 0)) {
		if (setgroups_control(pid, SETGROUPS_DENY) < 0 ||
		    map_id(pid, "group", "gid_map", gid, caller_gid) < 0 ||
		    map_id(pid, "user",  "uid_map", uid, caller_uid) < 0)
			return EX_OSERR;
	}

	if (send_cmd(sock, CMD_EXEC) < 0)
		errx(EX_IOERR, NULL);

	while (1) {
		pid_t cpid = wait(&status);
		if (cpid < 0)
			err(EX_OSERR, "waitpid");
		if (cpid == pid)
			break;
	}

	if (verbose > 1)
		warnx("container finished");

	return WEXITSTATUS(status);
}

static int main_child(int sock, char **argv)
{
	cmd_t cmd;

	__progname = (char *) "isolate: child";

	if ((cmd = recv_cmd(sock)) != CMD_FORK)
		errx(EX_IOERR, "unexpected message: %s", cmd2str(cmd));

	// unshare namespaces
	unshare_namespaces(unshare_flags);

	// fork to switch to new pid namespace
	pid_t pid = fork();

	if (pid < 0)
		err(EX_OSERR, "fork(second)");

	if (pid > 0) {
		if (send_cmd(sock, CMD_PID) < 0)
			errx(EX_OSERR, NULL);
		if (TEMP_FAILURE_RETRY(write(sock, &pid, sizeof(pid))) < 0)
			err(EX_OSERR, "unable to transfer pid");
		exit(EXIT_SUCCESS);
	}

	if ((cmd = recv_cmd(sock)) != CMD_EXEC)
		errx(EX_IOERR, "unexpected message: %s", cmd2str(cmd));

	if (prctl(PR_SET_PDEATHSIG, SIGKILL) < 0)
		err(EX_OSERR, "prctl(PR_SET_PDEATHSIG)");

	const char *cwd = NULL;

	if (rootdir) {
		if (chroot(rootdir) < 0)
			err(EX_SOFTWARE, "chroot: %s", rootdir);
		cwd = "/";
	}

	if (cwd && chdir(cwd) < 0)
		err(EX_SOFTWARE, "chdir: %s", cwd);

	if (gid >= 0 && setregid((gid_t) gid, (gid_t) gid) < 0)
		err(EX_SOFTWARE, "setregid");

	if (uid >= 0 && setreuid((uid_t) uid, (uid_t) uid) < 0)
		err(EX_SOFTWARE, "setreuid");

	cloexec_fds();

	execvp(argv[0], argv);
	warn("execvp");

	return EX_OSERR;
}

int main(int argc, char **argv)
{
	if (parse_arguments(argc, argv) < 0)
		return EX_USAGE;

	// enforce namespaces
	if (parse_unshare_namespaces(&unshare_flags, "user,pid,mount") < 0)
		return EX_CONFIG;

	// initialize data related to caller
	if (init_caller_data() < 0)
		return EX_NOUSER;

	int sock[2];

	if (ipc_pair(sock) < 0)
		err(EX_OSERR, "socketpair");

	pid_t pid = fork();

	if (pid < 0)
		err(EX_OSERR, "fork");

	if (pid > 0) {
		close(sock[1]);

		return main_parent(sock[0], pid);
	}
	close(sock[0]);

	return main_child(sock[1], &argv[optind]);
}
