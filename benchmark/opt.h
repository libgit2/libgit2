/*
 * Copyright (c), Edward Thomson <ethomson@edwardthomson.com>
 * All rights reserved.
 *
 * This file is part of adopt, distributed under the MIT license.
 * For full terms and conditions, see the included LICENSE file.
 */

#ifndef OPT_H
#define OPT_H

#include <stdio.h>

/**
 * The type of argument to be parsed.
 */
typedef enum {
	GITBENCH_OPT_NONE = 0,

	/** An argument that is specified ("--help" or "--debug") */
	GITBENCH_OPT_SWITCH,

	/** An argument that has a value ("--name value" or "-n value") */
	GITBENCH_OPT_VALUE,

	/** The literal arguments follow specifier, bare "--" */
	GITBENCH_OPT_LITERAL,

	/** A single "free" argument ("path") */
	GITBENCH_OPT_ARG,

	/** Unmatched arguments, a collection of "free" arguments ("paths...") */
	GITBENCH_OPT_ARGS,
} gitbench_opt_type_t;

/**
 * Usage information for an argument, to be displayed to the end-user.
 * This is only for display, the parser ignores this usage information.
 */
typedef enum {
	/** This argument is required. */
	GITBENCH_OPT_USAGE_REQUIRED = (1u << 0),

	/** A value is required for this argument. */
	GITBENCH_OPT_USAGE_VALUE_REQUIRED = (1u << 1),

	/** This argument should not be displayed in usage. */
	GITBENCH_OPT_USAGE_HIDDEN = (1u << 2),

	/** This is a multiple choice argument, combined with the previous arg. */
	GITBENCH_OPT_USAGE_CHOICE = (1u << 3),
} gitbench_opt_usage_t;

/** Specification for an available option. */
typedef struct gitbench_opt_spec {
	/** Type of option expected. */
	gitbench_opt_type_t type;

	/** Name of the long option. */
	const char *name;

	/** The alias is the short (one-character) option alias. */
	const char alias;

	/** The name of the value, provided when creating usage information. */
	const char *value;

	/**
	 * Short description of the option, used when creating usage
	 * information.
	 */
	const char *help;

	/**
	 * Optional `gitbench_opt_usage_t`, used when creating usage information.
	 */
	gitbench_opt_usage_t usage;
} gitbench_opt_spec;

/** An option provided on the command-line. */
typedef struct gitbench_opt {
	/**
	 * The specification that was provided on the command-line, or 
	 * `NULL` if the argument did not match an `gitbench_opt_spec`.
	 */
	const gitbench_opt_spec *spec;

	/**
	 * The value provided to the argument, or `NULL` if the given
	 * argument is a switch argument that does not take a value.
	 * If the argument did not match and `gitbench_opt_spec`, this will
	 * point to the unknown argument.
	 */
	const char *value;
} gitbench_opt;

/**
 * The gitbench_opt_parser structure.  Callers should not modify these values
 * directory.
 */
typedef struct gitbench_opt_parser {
	const gitbench_opt_spec *specs;
	const char **args;
	size_t args_len;

	size_t idx;
	size_t arg_idx;
	int in_literal : 1,
		in_short : 1;
} gitbench_opt_parser;

/**
 * Initializes a parser that parses the given arguments according to the
 * given specifications.
 *
 * @param parser The `gitbench_opt_parser` that will be initialized
 * @param specs A NULL-terminated array of `gitbench_opt_spec`s that can be parsed
 * @param argv The arguments that will be parsed
 * @param args_len The length of arguments to be parsed
 */
void gitbench_opt_parser_init(
	gitbench_opt_parser *parser,
	const gitbench_opt_spec specs[],
	const char **argv,
	size_t args_len);

/**
 * Parses the next command-line argument and places the information about
 * the argument into the given `opt` data.
 * 
 * @param opt The `gitbench_opt` information parsed from the argument
 * @param parser An `gitbench_opt_parser` that has been initialized with
 *        `gitbench_opt_parser_init`
 * @return true if the caller should continue iterating, or 0 if there are
 *         no arguments left to process.
 */
int gitbench_opt_parser_next(
	gitbench_opt *opt,
	gitbench_opt_parser *parser);

/**
 * Prints usage information to the given file handle.
 *
 * @param file The file to print information to
 * @param command The name of the command to use when printing
 * @param specs The specifications allowed by the command
 * @return 0 on success, -1 on failure
 */
int gitbench_opt_usage_fprint(
	FILE *file,
	const char *command,
	const gitbench_opt_spec specs[]);

#endif /* OPT_H */
