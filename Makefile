PROG = isolate
VERSION = 0.0.1

CFLAGS = \
	-pipe -O2 \
	-Wall -Wextra -W -Wshadow -Wcast-align \
	-Wwrite-strings -Wconversion -Waggregate-return -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn \
	-Wmissing-format-attribute -Wredundant-decls -Wdisabled-optimization

CPPFLAGS = -std=gnu99 -D_GNU_SOURCE=1 -DVERSION=\"$(VERSION)\"

SRCS = isolate.c caller.c fds.c ipc.c unshare.c
DEPS = $(SRCS:.c=.d)
OBJS = $(SRCS:.c=.o)

all: $(PROG)

clean:
	$(RM) -- $(PROG) $(DEPS) $(OBJS)

$(OBJS): $(DEPS)

$(PROG): $(OBJS)
	$(LINK.o) $^ -o $@


# We need dependencies only if goal isn't "clean".
ifneq ($(MAKECMDGOALS),clean)

%.d: %.c Makefile
	$(CC) -MM $(CPPFLAGS) $< |sed -e 's,\($*\)\.o[ :]*,\1.o $@: Makefile ,g' >$@

ifneq ($(DEPS),)
-include $(DEPS)
endif

endif # clean
