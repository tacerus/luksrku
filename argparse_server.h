/*
 *   This file was AUTO-GENERATED by pypgmopts.
 *
 *   https://github.com/johndoe31415/pypgmopts
 *
 *   Do not edit it by hand, your changes will be overwritten.
 *
 *   Generated at: 2021-06-27 13:24:40
 */

#ifndef __ARGPARSE_SERVER_H__
#define __ARGPARSE_SERVER_H__

#include <stdbool.h>

#define ARGPARSE_SERVER_DEFAULT_PORT		23170
#define ARGPARSE_SERVER_DEFAULT_VERBOSE		0

#define ARGPARSE_SERVER_NO_OPTION		0
#define ARGPARSE_SERVER_POSITIONAL_ARG	1

enum argparse_server_option_t {
	ARG_SERVER_PORT = 2,
	ARG_SERVER_SILENT = 3,
	ARG_SERVER_VERBOSE = 4,
	ARG_SERVER_FILENAME = 5,
};

typedef void (*argparse_server_errmsg_callback_t)(const char *errmsg, ...);
typedef void (*argparse_server_errmsg_option_callback_t)(enum argparse_server_option_t error_option, const char *errmsg, ...);
typedef bool (*argparse_server_callback_t)(enum argparse_server_option_t option, const char *value, argparse_server_errmsg_callback_t errmsg_callback);
typedef bool (*argparse_server_plausibilization_callback_t)(argparse_server_errmsg_option_callback_t errmsg_callback);

bool argparse_server_parse(int argc, char **argv, argparse_server_callback_t argument_callback, argparse_server_plausibilization_callback_t plausibilization_callback);
void argparse_server_show_syntax(void);
void argparse_server_parse_or_quit(int argc, char **argv, argparse_server_callback_t argument_callback, argparse_server_plausibilization_callback_t plausibilization_callback);

#endif
