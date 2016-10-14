/*
 * Log Functions
 *
 * Written by: Andrew Kilpatrick
 * Copyright 2014: Kilpatrick Audio
 *
 * This file is part of CARBON.
 *
 * CARBON is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CARBON is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CARBON.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include "../config.h"

#ifndef LOG_PRINT_ENABLE
#warning log printing disabled - no output will be sent
#endif

// state
char log_str[1024];

// local functions
void log_vstdout(char *prefix, char *fmt, va_list argp);
void log_vstderr(char *prefix, char *fmt, va_list argp);

// init the log
void log_init(void) {
}

// write a debug message
void log_debug(char *fmt, ...) {
#ifdef LOG_PRINT_ENABLE
    va_list argp;
    va_start(argp, fmt);
    log_vstdout("D:", fmt, argp);
    va_end(argp);
#endif
}

// write an info message
void log_info(char *fmt, ...) {
#ifdef LOG_PRINT_ENABLE
    va_list argp;
    va_start(argp, fmt);
    log_vstdout("I:", fmt, argp);
    va_end(argp);
#endif
}

// write a warning message
void log_warn(char *fmt, ...) {
#ifdef LOG_PRINT_ENABLE
    va_list argp;
    va_start(argp, fmt);
    log_vstdout("W:", fmt, argp);
    va_end(argp);
#endif
}

// write an error message
void log_error(char *fmt, ...) {
#ifdef LOG_PRINT_ENABLE
    va_list argp;
    va_start(argp, fmt);
    log_vstderr("E:", fmt, argp);
    va_end(argp);
#endif
}

// print a prefix and argument list to stdout
void log_vstdout(char *prefix, char *fmt, va_list argp) {
    vsprintf(log_str, fmt, argp);
    printf("%s %s\n", prefix, log_str);
}

// print a prefix and argument list to stderr
void log_vstderr(char *prefix, char *fmt, va_list argp) {
    vsprintf(log_str, fmt, argp);
    printf("%s %s\n", prefix, log_str);
}
