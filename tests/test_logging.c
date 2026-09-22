#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#include "logging.h"

static int g_openlog_calls;
static int g_syslog_calls;

void __wrap_openlog(const char* ident, int option, int facility) {
    (void)ident;
    (void)option;
    (void)facility;
    g_openlog_calls++;
}

void __wrap_closelog(void) {
}

void __wrap_syslog(int priority, const char* format, ...) {
    (void)priority;
    (void)format;
    g_syslog_calls++;
}

static void fill_file(const char* path, size_t size) {
    FILE* fp = fopen(path, "w");
    assert(fp != NULL);
    for (size_t i = 0; i < size; ++i) {
        assert(fputc('x', fp) != EOF);
    }
    assert(fclose(fp) == 0);
}

static void test_log_path_is_owned(void) {
    char path[] = "/tmp/minieap-log-path.XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);

    char* dynamic_path = strdup(path);
    assert(dynamic_path != NULL);
    set_log_file_path(dynamic_path);
    memset(dynamic_path, 'x', strlen(dynamic_path));
    free(dynamic_path);

    set_log_destination(LOG_TO_FILE);
    start_log();
    print_log("I", "", "owned-path");
    close_log();

    FILE* fp = fopen(path, "r");
    assert(fp != NULL);
    char line[256] = {0};
    assert(fgets(line, sizeof(line), fp) != NULL);
    assert(strstr(line, "owned-path") != NULL);
    fclose(fp);
    unlink(path);
}

static void test_log_none_disables_syslog(void) {
    set_log_file_path("none");
    set_log_destination(LOG_TO_CONSOLE);
    start_log();

    g_openlog_calls = 0;
    g_syslog_calls = 0;

    set_log_destination(LOG_NONE);
    print_log("I", "", "silent");
    close_log();

    assert(g_openlog_calls == 0);
    assert(g_syslog_calls == 0);
}

static void test_rotation_failure_keeps_old_stream(void) {
    char dir[] = "/tmp/minieap-log-dir.XXXXXX";
    assert(mkdtemp(dir) != NULL);

    char moved_dir[sizeof(dir) + 8];
    char path[sizeof(dir) + 16];
    snprintf(moved_dir, sizeof(moved_dir), "%s.moved", dir);
    snprintf(path, sizeof(path), "%s/log", dir);
    fill_file(path, 257 * 1024);

    set_log_file_path(path);
    set_log_destination(LOG_TO_FILE);
    start_log();
    assert(rename(dir, moved_dir) == 0);

    g_syslog_calls = 0;
    print_log("I", "", "after-failed-rotation");
    assert(g_syslog_calls >= 1);
    close_log();

    snprintf(path, sizeof(path), "%s/log", moved_dir);
    unlink(path);
    rmdir(moved_dir);
}

static void test_log_open_failure_is_reported(void) {
    char dir[] = "/tmp/minieap-missing-log-dir.XXXXXX";
    assert(mkdtemp(dir) != NULL);

    char path[sizeof(dir) + 8];
    snprintf(path, sizeof(path), "%s/log", dir);
    assert(rmdir(dir) == 0);

    set_log_destination(LOG_TO_FILE);
    set_log_file_path(path);
    g_syslog_calls = 0;
    start_log();
    assert(g_syslog_calls >= 1);
    close_log();
}

int main(void) {
    test_log_path_is_owned();
    test_log_none_disables_syslog();
    test_rotation_failure_keeps_old_stream();
    test_log_open_failure_is_reported();
    return 0;
}
