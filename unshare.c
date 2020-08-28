#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <err.h>
#include <sched.h> // CLONE_*
#include <fcntl.h>

#include "isolate.h"

#define PROC_ROOT "/proc"

static const char *setgroups_strings[] =
{
	[SETGROUPS_DENY]  = "deny",
	[SETGROUPS_ALLOW] = "allow",
};

static struct {
	const char *name;
	const char *clone_name;
	const int flag;
} const clone_flags[] = {
	{ "user", "CLONE_NEWUSER", CLONE_NEWUSER },
	{ "mount", "CLONE_NEWNS", CLONE_NEWNS },
	{ "filesystem", "CLONE_FS", CLONE_FS },
	{ "uts", "CLONE_NEWUTS", CLONE_NEWUTS },
	{ "ipc", "CLONE_NEWIPC", CLONE_NEWIPC },
	{ "net", "CLONE_NEWNET", CLONE_NEWNET },
	{ "pid", "CLONE_NEWPID", CLONE_NEWPID },
	{ "sysvsem", "CLONE_SYSVSEM", CLONE_SYSVSEM },
#ifdef CLONE_NEWCGROUP
	{ "cgroup", "CLONE_NEWCGROUP", CLONE_NEWCGROUP },
#endif
#ifdef CLONE_NEWTIME
	{ "time", "CLONE_NEWTIME", CLONE_NEWTIME }
#endif
};

static char filename[PATH_MAX];

static int add_flag(int *flags, const char *name)
{
	size_t i;

	if (!strncasecmp("all", name, 3)) {
		for (i = 0; i < ARRAY_SIZE(clone_flags); i++)
			*flags |= clone_flags[i].flag;
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(clone_flags); i++) {
		if (!strncasecmp(name, clone_flags[i].name, strlen(clone_flags[i].name))) {
			*flags |= clone_flags[i].flag;
			return 0;
		}
	}

	warnx("unknown unshare flag: %s", name);
	return 0;
}

int parse_unshare_namespaces(int *flags, const char *arg)
{
	int rc = 0;
	char *value;

	if (!(value = strdup(arg))) {
		warn("strdup");
		return -1;
	}

	while (value) {
		char *ptr = strchr(value, ',');

		if (ptr != NULL)
			*ptr++ = '\0';

		if (*value != '\0' && (rc = add_flag(flags, value)) < 0)
			break;

		value = ptr;
	}

	free(value);

	return rc;
}

int unshare_namespaces(const int flags)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(clone_flags); i++) {
		if (flags & clone_flags[i].flag) {
			if (verbose > 1)
				warnx("unshare namespace %s (%s)", clone_flags[i].name, clone_flags[i].clone_name);

			if (unshare(clone_flags[i].flag) < 0) {
				warn("unshare(%s)", clone_flags[i].clone_name);
				return -1;
			}
		}
	}
	return 0;
}

char *uid_mapping = NULL;
char *gid_mapping = NULL;

int parse_mapping(char **mapping, char *value)
{
	int i = 0;
	char *v;
	char *p = value;

	while (*p != 0) {
		v = p;
		errno = 0;
		strtoul(v, &p, 10);

		if (errno == ERANGE) {
			warn("bad value: %s", value);
			return -1;
		}

		if (*p != ':' && *p != 0) {
			warnx("wrong delimiter '%c'", *p);
			return -1;
		}

		if (*p == ':')
			*p = ' ';

		i++;
	}

	if (i != 3) {
		warnx("wrong number of IDs: %d", i);
		return -1;
	}

	if (*mapping) {
		size_t len = strlen(*mapping);

		v = *mapping = realloc(*mapping, len + 1 + strlen(value) + 1);
		v += len;
		*v++ = '\n';
	} else {
		v = *mapping = malloc(strlen(value) + 1);
	}

	strcpy(v, value);
	return 0;
}

static int map_id(pid_t pid, const char *type, const char *name,
		const char *mapping)
{
	int fd, rc = -1;
	size_t map_len;

	if (verbose > 1) {
		const char *m  = mapping;
		while (1) {
			char *p = strchr(m, '\n');
			if (p) {
				p++;
				warnx("remap %s %.*s", type, (int) (p - m - 1), m);
				m = p;
			} else {
				warnx("remap %s %s", type, m);
				break;
			}
		}
	}

	snprintf(filename, sizeof(filename), PROC_ROOT "/%d/%s", pid, name);

	if ((fd = open(filename, O_WRONLY)) < 0) {
		warn("open: %s", filename);
		goto end;;
	}

	map_len = strlen(mapping);

	if (write(fd, mapping, map_len) != (ssize_t) map_len) {
		warn("unable to write to %s", filename);
		goto end;
	}

	rc = 0;
end:
	close(fd);
	return rc;
}

static int setgroups_control(pid_t pid, const int action)
{
	int rc = -1;

	if (action < 0 || (size_t) action >= ARRAY_SIZE(setgroups_strings))
		return 0;

	const char *value = setgroups_strings[action];

	if (verbose > 1)
		warnx("set setgroups to %s", value);

	snprintf(filename, sizeof(filename), PROC_ROOT "/%d/setgroups", pid);

	int fd = open(filename, O_WRONLY);

	if (fd < 0) {
		warn("open: %s", filename);
		goto end;
	}

	if (dprintf(fd, "%s\n", value) < 0) {
		warnx("unable to write to %s", filename);
		goto end;
	}

	rc = 0;
end:
	close(fd);
	return rc;
}

int apply_id_mappings(pid_t pid)
{
	int rc;
	char *mapping;
	static char buf[1024];

	if ((rc = setgroups_control(pid, SETGROUPS_DENY)) < 0)
		return rc;

	mapping = gid_mapping;

	if (!mapping) {
		snprintf(buf, sizeof(buf), "%d %d 1", caller_gid, caller_gid);
		mapping = buf;
	}
	if ((rc = map_id(pid, "group", "gid_map", mapping)) < 0)
		return rc;

	mapping = uid_mapping;

	if (!mapping) {
		snprintf(buf, sizeof(buf), "%d %d 1", caller_uid, caller_uid);
		mapping = buf;
	}
	if ((rc = map_id(pid, "user", "uid_map", mapping)) < 0)
		return rc;

	return 0;
}
