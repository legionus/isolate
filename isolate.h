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

/* unshare.c */
extern int parse_unshare_namespaces(int *flags, const char *arg);
extern int unshare_namespaces(const int flags);
extern int map_id(pid_t pid, const char *type, const char *name,
		const long from, const long to);
extern int setgroups_control(pid_t pid, const int action);

#endif /* _ISOLATE_H_ */
