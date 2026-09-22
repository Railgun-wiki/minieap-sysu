CC ?= clang
CFLAGS ?= -O1 -g -Wall -Wextra -Wpedantic
SANITIZERS ?= -fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all clean
all: test_logging test_retry_policy
	./test_logging
	./test_retry_policy

test_logging: test_logging.c ../util/logging.c ../include/logging.h
	$(CC) $(CFLAGS) $(SANITIZERS) -I../include \
		-Wl,--wrap=openlog -Wl,--wrap=closelog -Wl,--wrap=syslog \
		test_logging.c ../util/logging.c -o $@

test_retry_policy: test_retry_policy.c ../include/retry_policy.h
	$(CC) $(CFLAGS) $(SANITIZERS) -I../include test_retry_policy.c -o $@

clean:
	rm -f test_logging test_retry_policy
