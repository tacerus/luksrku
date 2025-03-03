/*
 *   This file was AUTO-GENERATED by pypgmopts.
 *
 *   https://github.com/johndoe31415/pypgmopts
 *
 *   Do not edit it by hand, your changes will be overwritten.
 *
 *   Generated at: 2021-06-27 13:24:40
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <string.h>
#include "argparse_client.h"

static enum argparse_client_option_t last_parsed_option;
static char last_error_message[256];
static const char *option_texts[] = {
	[ARG_CLIENT_TIMEOUT] = "-t / --timeout",
	[ARG_CLIENT_PORT] = "-p / --port",
	[ARG_CLIENT_NO_LUKS] = "--no-luks",
	[ARG_CLIENT_VERBOSE] = "-v / --verbose",
	[ARG_CLIENT_FILENAME] = "filename",
	[ARG_CLIENT_HOSTNAME] = "hostname",
};

enum argparse_client_option_internal_t {
	ARG_CLIENT_TIMEOUT_SHORT = 't',
	ARG_CLIENT_PORT_SHORT = 'p',
	ARG_CLIENT_VERBOSE_SHORT = 'v',
	ARG_CLIENT_TIMEOUT_LONG = 1000,
	ARG_CLIENT_PORT_LONG = 1001,
	ARG_CLIENT_NO_LUKS_LONG = 1002,
	ARG_CLIENT_VERBOSE_LONG = 1003,
	ARG_CLIENT_FILENAME_LONG = 1004,
	ARG_CLIENT_HOSTNAME_LONG = 1005,
};

static void errmsg_callback(const char *errmsg, ...) {
	va_list ap;
	va_start(ap, errmsg);
	vsnprintf(last_error_message, sizeof(last_error_message), errmsg, ap);
	va_end(ap);
}

static void errmsg_option_callback(enum argparse_client_option_t error_option, const char *errmsg, ...) {
	last_parsed_option = error_option;

	va_list ap;
	va_start(ap, errmsg);
	vsnprintf(last_error_message, sizeof(last_error_message), errmsg, ap);
	va_end(ap);
}

bool argparse_client_parse(int argc, char **argv, argparse_client_callback_t argument_callback, argparse_client_plausibilization_callback_t plausibilization_callback) {
	last_parsed_option = ARGPARSE_CLIENT_NO_OPTION;
	const char *short_options = "t:p:v";
	struct option long_options[] = {
		{ "timeout",                          required_argument, 0, ARG_CLIENT_TIMEOUT_LONG },
		{ "port",                             required_argument, 0, ARG_CLIENT_PORT_LONG },
		{ "no-luks",                          no_argument, 0, ARG_CLIENT_NO_LUKS_LONG },
		{ "verbose",                          no_argument, 0, ARG_CLIENT_VERBOSE_LONG },
		{ "filename",                         required_argument, 0, ARG_CLIENT_FILENAME_LONG },
		{ "hostname",                         required_argument, 0, ARG_CLIENT_HOSTNAME_LONG },
		{ 0 }
	};

	while (true) {
		int optval = getopt_long(argc, argv, short_options, long_options, NULL);
		if (optval == -1) {
			break;
		}
		last_error_message[0] = 0;
		enum argparse_client_option_internal_t arg = (enum argparse_client_option_internal_t)optval;
		switch (arg) {
			case ARG_CLIENT_TIMEOUT_SHORT:
			case ARG_CLIENT_TIMEOUT_LONG:
				last_parsed_option = ARG_CLIENT_TIMEOUT;
				if (!argument_callback(ARG_CLIENT_TIMEOUT, optarg, errmsg_callback)) {
					return false;
				}
				break;

			case ARG_CLIENT_PORT_SHORT:
			case ARG_CLIENT_PORT_LONG:
				last_parsed_option = ARG_CLIENT_PORT;
				if (!argument_callback(ARG_CLIENT_PORT, optarg, errmsg_callback)) {
					return false;
				}
				break;

			case ARG_CLIENT_NO_LUKS_LONG:
				last_parsed_option = ARG_CLIENT_NO_LUKS;
				if (!argument_callback(ARG_CLIENT_NO_LUKS, optarg, errmsg_callback)) {
					return false;
				}
				break;

			case ARG_CLIENT_VERBOSE_SHORT:
			case ARG_CLIENT_VERBOSE_LONG:
				last_parsed_option = ARG_CLIENT_VERBOSE;
				if (!argument_callback(ARG_CLIENT_VERBOSE, optarg, errmsg_callback)) {
					return false;
				}
				break;

			default:
				last_parsed_option = ARGPARSE_CLIENT_NO_OPTION;
				errmsg_callback("unrecognized option supplied");
				return false;
		}
	}

	const int positional_argument_cnt = argc - optind;
	const int flexible_positional_args_cnt = positional_argument_cnt - 1;
	last_parsed_option = ARGPARSE_CLIENT_POSITIONAL_ARG;
	if (positional_argument_cnt < 1) {
		errmsg_callback("expected a minimum of 1 positional argument, but %d given.", positional_argument_cnt);
		return false;
	}
	if (positional_argument_cnt > 2) {
		errmsg_callback("expected a maximum of 2 positional arguments, but %d given.", positional_argument_cnt);
		return false;
	}

	int positional_index = optind;
	last_parsed_option = ARG_CLIENT_FILENAME;
	if (!argument_callback(ARG_CLIENT_FILENAME, argv[positional_index++], errmsg_callback)) {
		return false;
	}
	last_parsed_option = ARG_CLIENT_HOSTNAME;
	for (int i = 0; i < flexible_positional_args_cnt; i++) {
		if (!argument_callback(ARG_CLIENT_HOSTNAME, argv[positional_index++], errmsg_callback)) {
			return false;
		}
	}

	if (plausibilization_callback) {
		if (!plausibilization_callback(errmsg_option_callback)) {
			return false;
		}
	}
	return true;
}

void argparse_client_show_syntax(void) {
	fprintf(stderr, "usage: luksrku client [-t secs] [-p port] [--no-luks] [-v] filename [hostname]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Connects to a luksrku key server and unlocks local LUKS volumes.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "positional arguments:\n");
	fprintf(stderr, "  filename              Exported database file to load TLS-PSKs and list of disks from.\n");
	fprintf(stderr, "  hostname              When hostname is given, auto-searching for suitable servers is disabled and\n");
	fprintf(stderr, "                        only a connection to the given hostname is attempted.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "optional arguments:\n");
	fprintf(stderr, "  -t secs, --timeout secs\n");
	fprintf(stderr, "                        When searching for a keyserver and not all volumes can be unlocked, abort\n");
	fprintf(stderr, "                        after this period of time, given in seconds. Defaults to infinity. This\n");
	fprintf(stderr, "                        argument can be specified as a host-based configuration parameter as well;\n");
	fprintf(stderr, "                        the command-line argument always takes precedence.\n");
	fprintf(stderr, "  -p port, --port port  Port that is used for both UDP and TCP communication. Defaults to 23170.\n");
	fprintf(stderr, "  --no-luks             Do not call LUKS/cryptsetup. Useful for testing unlocking procedure.\n");
	fprintf(stderr, "  -v, --verbose         Increase verbosity. Can be specified multiple times.\n");
}

void argparse_client_parse_or_quit(int argc, char **argv, argparse_client_callback_t argument_callback, argparse_client_plausibilization_callback_t plausibilization_callback) {
	if (!argparse_client_parse(argc, argv, argument_callback, plausibilization_callback)) {
		if (last_parsed_option > ARGPARSE_CLIENT_POSITIONAL_ARG) {
			if (last_error_message[0]) {
				fprintf(stderr, "luksrku client: error parsing argument %s -- %s\n", option_texts[last_parsed_option], last_error_message);
			} else {
				fprintf(stderr, "luksrku client: error parsing argument %s -- no details available\n", option_texts[last_parsed_option]);
			}
		} else if (last_parsed_option == ARGPARSE_CLIENT_POSITIONAL_ARG) {
			fprintf(stderr, "luksrku client: error parsing optional arguments -- %s\n", last_error_message);
		}
		argparse_client_show_syntax();
		exit(EXIT_FAILURE);
	}
}

#ifdef __ARGPARSE_MAIN__
/*   gcc -D __ARGPARSE_MAIN__ -O2 -Wall -o argparse argparse_client.c
*/

static const char *option_enum_to_str(enum argparse_client_option_t option) {
	switch (option) {
		case ARG_CLIENT_TIMEOUT: return "ARG_CLIENT_TIMEOUT";
		case ARG_CLIENT_PORT: return "ARG_CLIENT_PORT";
		case ARG_CLIENT_NO_LUKS: return "ARG_CLIENT_NO_LUKS";
		case ARG_CLIENT_VERBOSE: return "ARG_CLIENT_VERBOSE";
		case ARG_CLIENT_FILENAME: return "ARG_CLIENT_FILENAME";
		case ARG_CLIENT_HOSTNAME: return "ARG_CLIENT_HOSTNAME";
	}
	return "UNKNOWN";
}

bool arg_print_callback(enum argparse_client_option_t option, const char *value, argparse_client_errmsg_callback_t errmsg_callback) {
	fprintf(stderr, "%s = \"%s\"\n", option_enum_to_str(option), value);
	return true;
}

int main(int argc, char **argv) {
	argparse_client_parse_or_quit(argc, argv, arg_print_callback, NULL);
	return 0;
}
#endif
