/* -*- Mode: C; tab-width: 4; -*- */
/*
* 文件名称：logging.c
* 摘	要：MiniEAP日志功能 (支持双路输出、syslog集成及LuCI配合)
* 作	者：updateing@HUST
* 邮	箱：haotia@gmail.com
*/

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>

#include "logging.h"

#define LOG_FORMAT_BUFFER_SIZE 1024
#define DEFAULT_LOG_FILE "/var/log/minieap.log"

static char g_time_buffer[64];
static char* g_log_path = DEFAULT_LOG_FILE;
static FILE* g_file_fp = NULL;
static LOG_DEST g_dest = LOG_TO_CONSOLE;
static int g_syslog_opened = 0;

static char* get_formatted_date() {
	time_t time_tmp;
	struct tm* time_s;

	time(&time_tmp);
	time_s = localtime(&time_tmp);

	snprintf(g_time_buffer, sizeof(g_time_buffer), "%d/%d/%d %d:%02d:%02d",
			time_s->tm_year + 1900, time_s->tm_mon + 1,
			time_s->tm_mday, time_s->tm_hour, time_s->tm_min, time_s->tm_sec);
	return g_time_buffer;
}

void set_log_destination(LOG_DEST dst) {
    g_dest = dst;
}

void set_log_file_path(char* path) {
    if (path == NULL) return;
    if (g_log_path != NULL && strcmp(g_log_path, path) == 0) return;

    if (g_file_fp != NULL) {
        fclose(g_file_fp);
        g_file_fp = NULL;
    }
    g_log_path = path;
    if (strcmp(g_log_path, "/dev/null") != 0 && strcmp(g_log_path, "none") != 0) {
        g_file_fp = fopen(g_log_path, "a");
        if (g_file_fp != NULL) {
            setvbuf(g_file_fp, NULL, _IOLBF, BUFSIZ);
        }
    }
}

void start_log() {
    if (!g_syslog_opened) {
        openlog("minieap", LOG_PID | LOG_NDELAY, LOG_DAEMON);
        g_syslog_opened = 1;
    }

    if (g_file_fp == NULL && g_log_path != NULL &&
        strcmp(g_log_path, "/dev/null") != 0 && strcmp(g_log_path, "none") != 0) {
        g_file_fp = fopen(g_log_path, "a");
        if (g_file_fp != NULL) {
            setvbuf(g_file_fp, NULL, _IOLBF, BUFSIZ);
        }
    }
}

void close_log() {
    if (g_file_fp != NULL) {
        fclose(g_file_fp);
        g_file_fp = NULL;
    }
    if (g_syslog_opened) {
        closelog();
        g_syslog_opened = 0;
    }
}

static void print_detail_line(const char* log_level, const char* func_name,
                              const char* log_format, va_list argptr) {
    char user_buffer[LOG_FORMAT_BUFFER_SIZE];
    char line_buffer[LOG_FORMAT_BUFFER_SIZE + 128];
    va_list arg_copy;

    va_copy(arg_copy, argptr);
    vsnprintf(user_buffer, sizeof(user_buffer), log_format, arg_copy);
    va_end(arg_copy);

    if (func_name != NULL && func_name[0] != 0) {
        snprintf(line_buffer, sizeof(line_buffer), "[%s][%s](%s) %s\n",
                 get_formatted_date(), log_level, func_name, user_buffer);
    } else {
        snprintf(line_buffer, sizeof(line_buffer), "[%s][%s] %s\n",
                 get_formatted_date(), log_level, user_buffer);
    }

    /* Output to Console (if foreground) */
    if (g_dest == LOG_TO_CONSOLE) {
        fputs(line_buffer, stdout);
        fflush(stdout);
    }

    /* Output to Log File (always keep /var/log/minieap.log updated for LuCI) */
    if (g_dest != LOG_NONE && g_file_fp != NULL) {
        long sz = ftell(g_file_fp);
        if (sz > 256 * 1024) {
            FILE* new_fp = freopen(g_log_path, "w", g_file_fp);
            if (new_fp != NULL) {
                g_file_fp = new_fp;
                fputs("[MiniEAP] 日志大小超过 256KB，已自动循环重置\n", g_file_fp);
            }
        }
        fputs(line_buffer, g_file_fp);
        fflush(g_file_fp);
    }

    /* Output to OpenWrt Syslog (for logread / LuCI System Log) */
    if (g_syslog_opened) {
        int priority = LOG_INFO;
        if (strcmp(log_level, "E") == 0) priority = LOG_ERR;
        else if (strcmp(log_level, "W") == 0) priority = LOG_WARNING;
        else if (strcmp(log_level, "D") == 0) priority = LOG_DEBUG;

        if (func_name != NULL && func_name[0] != 0) {
            syslog(priority, "[%s](%s) %s", log_level, func_name, user_buffer);
        } else {
            syslog(priority, "[%s] %s", log_level, user_buffer);
        }
    }
}

static void print_raw_line(const char* log_format, va_list argptr) {
    char user_buffer[LOG_FORMAT_BUFFER_SIZE];
    va_list arg_copy;

    va_copy(arg_copy, argptr);
    vsnprintf(user_buffer, sizeof(user_buffer), log_format, arg_copy);
    va_end(arg_copy);

    if (g_dest == LOG_TO_CONSOLE) {
        fputs(user_buffer, stdout);
        fflush(stdout);
    }
    if (g_dest != LOG_NONE && g_file_fp != NULL) {
        fputs(user_buffer, g_file_fp);
        fflush(g_file_fp);
    }
}

void print_log(const char* log_level, const char* func_name, const char* log_format, ...) {
	va_list argptr;
	va_start(argptr, log_format);
	print_detail_line(log_level, func_name, log_format, argptr);
	va_end(argptr);
}

void print_log_raw(const char* log_format, ...) {
	va_list argptr;
	va_start(argptr, log_format);
	print_raw_line(log_format, argptr);
	va_end(argptr);
}
