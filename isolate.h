#ifndef _ISOLATE_H_
#define _ISOLATE_H_

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

enum {
	SETGROUPS_NONE  = -1,
	SETGROUPS_DENY  = 0,
	SETGROUPS_ALLOW = 1,
};

typedef enum {
	CMD_INVALID  = 0,
	CMD_FORK,
	CMD_PID,
	CMD_EXEC
} cmd_t;

extern int verbose;

/* caller.c */
extern char *caller_user;
extern uid_t caller_uid;
extern gid_t caller_gid;

extern int init_caller_data(void);

/* fds.c */
extern void cloexec_fds(void);

/* ipc.c */
extern const char *cmd2str(cmd_t cmd);
extern int ipc_pair(int sv[2]);
extern int send_cmd(int fd, cmd_t cmd);
extern cmd_t recv_cmd(int fd);

/* rlimit.c */
extern int parse_rlimits(char *value);
extern void change_rlimits(void);

/* unshare.c */
extern char *uid_mapping;
extern char *gid_mapping;

extern int parse_unshare_namespaces(int *flags, const char *arg);
extern int unshare_namespaces(const int flags);
extern int parse_mapping(char **mapping, char *value);
extern int apply_id_mappings(pid_t pid);

static inline int parse_uid_mapping(char *value)
{
	return parse_mapping(&uid_mapping, value);
}

static inline int parse_gid_mapping(char *value)
{
	return parse_mapping(&gid_mapping, value);
}

#endif /* _ISOLATE_H_ */
