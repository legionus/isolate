/*
  Copyright (C) 2003-2019  Dmitry V. Levin <ldv@altlinux.org>

  The caller data initialization module for the hasher-priv program.

  SPDX-License-Identifier: GPL-2.0-or-later
*/

/* Code in this file may be executed with root privileges. */

#include <errno.h>
#include <error.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <err.h>

#include "isolate.h"

char *caller_user = NULL;
uid_t caller_uid;
gid_t caller_gid;

/*
 * Initialize caller_user, caller_uid, caller_gid and caller_home.
 */
int init_caller_data(void)
{
	struct passwd *pw = NULL;

	caller_uid = getuid();
	caller_gid = getgid();

	pw = getpwuid(caller_uid);

	if (!pw || !pw->pw_name) {
		warnx("caller lookup failure");
		return -1;
	}

	caller_user = strdup(pw->pw_name);

	if (!caller_user) {
		warnx("strdup");
		return -1;
	}

	if (caller_uid != pw->pw_uid) {
		warnx("caller %s: uid mismatch", caller_user);
		return -1;
	}

	if (caller_gid != pw->pw_gid) {
		warnx("caller %s: gid mismatch", caller_user);
		return -1;
	}

	return 0;
}
