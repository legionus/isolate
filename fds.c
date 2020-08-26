#include <linux/limits.h>

#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sysexits.h>
#include <errno.h>
#include <err.h>

#include "isolate.h"

static int get_open_max(void)
{
	long int i = sysconf(_SC_OPEN_MAX);

	if (i < NR_OPEN)
		i = NR_OPEN;
	if (i > INT_MAX)
		i = INT_MAX;

	return (int) i;
}

void cloexec_fds(void)
{
	int fd, max_fd = get_open_max();

	/* Set close-on-exec flag on all non-standard descriptors. */
	for (fd = STDERR_FILENO + 1; fd < max_fd; ++fd) {
		int flags = fcntl(fd, F_GETFD, 0);

		if (flags < 0)
			continue;

		int newflags = flags | FD_CLOEXEC;

		if (flags != newflags && fcntl(fd, F_SETFD, newflags))
			err(EX_SOFTWARE, "fcntl(F_SETFD)");
	}

	errno = 0;
}

