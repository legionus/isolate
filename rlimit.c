#include <sys/time.h>
#include <sys/resource.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include "isolate.h"

struct rlimit_parameter {
	const char *name;
	int resource;
	rlim_t soft;
	rlim_t hard;
};

struct rlimit_parameter rlimit_config[] = {
	{ .name = "as",      .resource = RLIMIT_AS,      .soft = 0, .hard = 0, },
	{ .name = "core",    .resource = RLIMIT_CORE,    .soft = 0, .hard = 0, },
	{ .name = "cpu",     .resource = RLIMIT_CPU,     .soft = 0, .hard = 0, },
	{ .name = "data",    .resource = RLIMIT_DATA,    .soft = 0, .hard = 0, },
	{ .name = "fsize",   .resource = RLIMIT_FSIZE,   .soft = 0, .hard = 0, },
	{ .name = "locks",   .resource = RLIMIT_LOCKS,   .soft = 0, .hard = 0, },
	{ .name = "memlock", .resource = RLIMIT_MEMLOCK, .soft = 0, .hard = 0, },
	{ .name = "nofile",  .resource = RLIMIT_NOFILE,  .soft = 0, .hard = 0, },
	{ .name = "nproc",   .resource = RLIMIT_NPROC,   .soft = 0, .hard = 0, },
	{ .name = "rss",     .resource = RLIMIT_RSS,     .soft = 0, .hard = 0, },
	{ .name = "stack",   .resource = RLIMIT_STACK,   .soft = 0, .hard = 0, },
	{ .name = NULL,      .resource = 0,              .soft = 0, .hard = 0, },
};

static int update_value(char *value)
{
	char *ptr = strchr(value, '=');

	if (ptr != NULL)
		*ptr++ = '\0';

	if (*value == '\0')
		return 0;

	for (struct rlimit_parameter *p = rlimit_config; p->name; p++) {
		char t = 0;

		if (!strncmp(value, "soft-", 5))
			t = 1;
		else if (!strncmp(value, "hard-", 5))
			t = 2;

		if (strcmp(p->name, value + (t ? 5 : 0)))
			continue;

		switch (t) {
			case 0:
				p->soft = strtoul(ptr, NULL, 10);
				p->hard = p->soft;
				break;
			case 1:
				p->soft = strtoul(ptr, NULL, 10);
				break;
			case 2:
				p->hard = strtoul(ptr, NULL, 10);
				break;
		}
	}

	return 0;
}

int parse_rlimits(char *value)
{
	int rc = 0;

	while (value) {
		char *ptr = strchr(value, ',');

		if (ptr != NULL)
			*ptr++ = '\0';

		if (*value != '\0' && (rc = update_value(value)) < 0)
			break;

		value = ptr;
	}

	return rc;
}

void change_rlimits(void)
{
	for (struct rlimit_parameter *p = rlimit_config; p->name; p++) {
		struct rlimit r;

		if (getrlimit(p->resource, &r) < 0)
			err(EXIT_FAILURE, "getrlimit(rlimit_%s)", p->name);

		if (p->soft)
			r.rlim_cur = p->soft;

		if (p->hard)
			r.rlim_max = p->hard;

		if (r.rlim_max < r.rlim_cur)
			r.rlim_max = r.rlim_cur;

		if (verbose > 2) {
			if (r.rlim_cur != RLIM_INFINITY && r.rlim_max != RLIM_INFINITY)
				warnx("setting rlimit %-8s: soft %-20lu hard %lu", p->name, r.rlim_cur, r.rlim_max);
			else if (r.rlim_cur != RLIM_INFINITY)
				warnx("setting rlimit %-8s: soft %-20lu hard %s",  p->name, r.rlim_cur, "unlimited");
			else if (r.rlim_max != RLIM_INFINITY)
				warnx("setting rlimit %-8s: soft %-20s hard %lu",  p->name, "unlimited", r.rlim_max);
			else
				warnx("setting rlimit %-8s: soft %-20s hard %s",   p->name, "unlimited", "unlimited");
		}

		if (setrlimit(p->resource, &r) < 0)
			err(EXIT_FAILURE, "setrlimit(rlimit_%s)", p->name);
	}
}
