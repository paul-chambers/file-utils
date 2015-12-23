CFLAGS += -Wall -Wextra

all: if-newer match-ext nzb-cleanup

clean:
	rm if-newer match-ext nzb-cleanup

if-newer: if-newer.c

match-ext: match-ext.c

nzb-cleanup: nzb-cleanup.c


