/*
 *   This file was AUTO-GENERATED by pypgmopts.
 *
 *   https://github.com/johndoe31415/pypgmopts
 *
 *   Do not edit it by hand, your changes will be overwritten.
 *
 *   Generated at: 2019-10-23 09:40:28
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <string.h>
#include "argparse_edit.h"

static enum argparse_edit_option_t last_parsed_option;
static char last_error_message[256];
static const char *option_texts[] = {
	[ARG_EDIT_VERBOSE] = "-v / --verbose",
	[ARG_EDIT_FILENAME] = "filename",
};

enum argparse_edit_option_internal_t {
	ARG_EDIT_VERBOSE_SHORT = 'v',
	ARG_EDIT_VERBOSE_LONG = 1000,
	ARG_EDIT_FILENAME_LONG = 1001,
};

static void errmsg_callback(const char *errmsg, ...) {
	va_list ap;
	va_start(ap, errmsg);
	vsnprintf(last_error_message, sizeof(last_error_message), errmsg, ap);
	va_end(ap);
}

static void errmsg_option_callback(enum argparse_edit_option_t error_option, const char *errmsg, ...) {
	last_parsed_option = error_option;

	va_list ap;
	va_start(ap, errmsg);
	vsnprintf(last_error_message, sizeof(last_error_message), errmsg, ap);
	va_end(ap);
}

bool argparse_edit_parse(int argc, char **argv, argparse_edit_callback_t argument_callback, argparse_edit_plausibilization_callback_t plausibilization_callback) {
	last_parsed_option = ARGPARSE_EDIT_NO_OPTION;
	const char *short_options = "v";
	struct option long_options[] = {
		{ "verbose",                          no_argument, 0, ARG_EDIT_VERBOSE_LONG },
		{ "filename",                         required_argument, 0, ARG_EDIT_FILENAME_LONG },
		{ 0 }
	};

	while (true) {
		int optval = getopt_long(argc, argv, short_options, long_options, NULL);
		if (optval == -1) {
			break;
		}
		last_error_message[0] = 0;
		enum argparse_edit_option_internal_t arg = (enum argparse_edit_option_internal_t)optval;
		switch (arg) {
			case ARG_EDIT_VERBOSE_SHORT:
			case ARG_EDIT_VERBOSE_LONG:
				last_parsed_option = ARG_EDIT_VERBOSE;
				if (!argument_callback(ARG_EDIT_VERBOSE, optarg, errmsg_callback)) {
					return false;
				}
				break;

			default:
				last_parsed_option = ARGPARSE_EDIT_NO_OPTION;
				errmsg_callback("unrecognized option supplied");
				return false;
		}
	}

	const int positional_argument_cnt = argc - optind;
	const int flexible_positional_args_cnt = positional_argument_cnt - 0;
	last_parsed_option = ARGPARSE_EDIT_POSITIONAL_ARG;
	if (positional_argument_cnt < 0) {
		errmsg_callback("expected a minimum of 0 positional arguments, but %d given.", positional_argument_cnt);
		return false;
	}
	if (positional_argument_cnt > 1) {
		errmsg_callback("expected a maximum of 1 positional argument, but %d given.", positional_argument_cnt);
		return false;
	}

	int positional_index = optind;
	last_parsed_option = ARG_EDIT_FILENAME;
	for (int i = 0; i < flexible_positional_args_cnt; i++) {
		if (!argument_callback(ARG_EDIT_FILENAME, argv[positional_index++], errmsg_callback)) {
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

void argparse_edit_show_syntax(void) {
	fprintf(stderr, "usage: luksrku edit [-v] [filename]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "positional arguments:\n");
	fprintf(stderr, "  filename       Database file to edit.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "optional arguments:\n");
	fprintf(stderr, "  -v, --verbose  Increase verbosity. Can be specified multiple times.\n");
}

void argparse_edit_parse_or_quit(int argc, char **argv, argparse_edit_callback_t argument_callback, argparse_edit_plausibilization_callback_t plausibilization_callback) {
	if (!argparse_edit_parse(argc, argv, argument_callback, plausibilization_callback)) {
		if (last_parsed_option > ARGPARSE_EDIT_POSITIONAL_ARG) {
			if (last_error_message[0]) {
				fprintf(stderr, "luksrku edit: error parsing argument %s -- %s\n", option_texts[last_parsed_option], last_error_message);
			} else {
				fprintf(stderr, "luksrku edit: error parsing argument %s -- no details available\n", option_texts[last_parsed_option]);
			}
		} else if (last_parsed_option == ARGPARSE_EDIT_POSITIONAL_ARG) {
			fprintf(stderr, "luksrku edit: error parsing optional arguments -- %s\n", last_error_message);
		}
		argparse_edit_show_syntax();
		exit(EXIT_FAILURE);
	}
}

#ifdef __ARGPARSE_MAIN__
/*   gcc -D __ARGPARSE_MAIN__ -O2 -Wall -o argparse argparse_edit.c
*/

static const char *option_enum_to_str(enum argparse_edit_option_t option) {
	switch (option) {
		case ARG_EDIT_VERBOSE: return "ARG_EDIT_VERBOSE";
		case ARG_EDIT_FILENAME: return "ARG_EDIT_FILENAME";
	}
	return "UNKNOWN";
}

bool arg_print_callback(enum argparse_edit_option_t option, const char *value, argparse_edit_errmsg_callback_t errmsg_callback) {
	fprintf(stderr, "%s = \"%s\"\n", option_enum_to_str(option), value);
	return true;
}

int main(int argc, char **argv) {
	argparse_edit_parse_or_quit(argc, argv, arg_print_callback, NULL);
	return 0;
}
#endif
