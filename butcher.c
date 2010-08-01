/* @@SOURCE-HEADER@@ */

#include "bt.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <pthread.h>

/*
 * WARNING: This is just a stupid example! Its only purpose is to show
 * how the API of butcher should look like.
 */

static const struct options {
	const char * long_name;
	char short_name;
	char need_arg;
	unsigned int id;
	const char * help;
} options[] = {
		{"match-suite",	's',	1,	1,
				"run only tests in matched suites; <arg> is a regex\n"
				"as povided by the POSIX2 specification (man 7 regex)"},
		{"match-test",	't',	1,	2,
			"run only matched tests; <arg> is a regex"},
		{"verbose",		'v',	0,	3,
			"be verbose, repeat for descriptions and messages (in order)"},
		{"quiet",		'q',	0,	4,
			"be quiet (default)"},
		{"color",		'c',	0,	5,
			"enable color output"},
		{"no-color",	'n',	0,	6,
			"disable color output (default)"},
		{"descriptor",	'd',	1,	7,
			"write to given descriptor instead of 1 (stdout)"},
		{"list",		'l',	0,	8,
			"instead of running tests, just dump everything available"},
		{"help",		'h',	0,	9,
			"display this screen"},
		{"usage",		0,		0,	10,
			"display this screen"},
		{"bexec",		'b',		1,	12,
			"use <arg> as path to bexec, e.g. /usr/bin/bexec"},
		{"debugger",		'g',		1,	11,
			"use <arg> as debugger for bexec, e.g. /usr/bin/valgrind"},
		{NULL,			0,		0,	0,	NULL}
};

void usage(int fd)
{
	unsigned int idx;

	dprintf(fd,
			"The BUTCHER unit test - runs test functions inside shared objects \n"
			"with the help of libelf and libdl. See \"bt.h\" for details.\n"
			"\n"
			"Usage: \n"
			"	export LD_LIBRARY_PATH=<path to link dependencies>\n"
			"	butcher <options> <shared-objects>\n");
	dprintf(fd,
			"Options: \n");

	for (idx = 0; options[idx].long_name; idx++) {
		if (options[idx].short_name)
			dprintf(fd,
					"	--%s%s (-%c%s)\n",
					options[idx].long_name, options[idx].need_arg?" <arg>":"",
					options[idx].short_name, options[idx].need_arg?" <arg>":"");
		else
			dprintf(fd,
					"	--%s %s\n", options[idx].long_name, options[idx].need_arg?"<arg>":"");

		const char * s = options[idx].help;
		const char * e = NULL;

		while (s && *s) {
			e = strchr(s, '\n');
			if (e) {
				dprintf(fd,
						"		%.*s\n", (int)(e-s), s);
			} else {
				dprintf(fd,
						"		%s\n", s);
			}
			s = (e) ? e + 1 : NULL;
		};
	}

	dprintf(fd,
			"Example: \n"
			"	butcher -cv -s '^ugly' -t '^important' libfoo.so libbar.so\n");
}

int main(int argc, char * argv[], char * env[]) {

	bt_t * butcher = NULL;
	int err;

	int paramc;
	char * paramv[argc+1];

	int i, fd, shortflag;
	size_t len;

	char * smatch, * tmatch;

	int list, help, verbose, color;

	unsigned int idx;

	char * argument, * bexec, * debugger;
	UNUSED_PARAM(env);

	fd = 1; /* stdout */

	err = bt_new(&butcher);
	if (err)
		return_error(err);

	smatch = NULL;
	tmatch = NULL;
	list = 0;
	help = 0;
	verbose = 0;
	color = 0;
	shortflag = 0;
	bexec = NULL;
	debugger = NULL;

	/*
	 * this IS a mess... but again: it is only an example
	 * TODO: maybe it would be wise to use a ragel state machine here, e.g.:
	 *
	 * %%{
	 *  machine opts;
	 *
	 *   short_option_tok = print -- '-';
	 *   short_option = '-' short_option_tok+;
	 *
	 *   long_option_value = print+;
	 *   long_option = (('--' (print - '-')((print+) -- ('--'))) -- '=') ( '=' (long_option_value -- '='))?;
	 *
	 *   option = (long_option | short_option);
	 *   parameter = (print - '-') print*;
	 *   arg = option | parameter;
	 *
	 *   main := arg;
	 * }%%
	 *
	 * the web site is http://www.complang.org/ragel/
	 */

	for (i = 1, paramc = 0; i < argc; i++) {
		argument = NULL;

		if ((len = strlen(argv[i])) > 0) {
			if (len == 1) {
				/* single char options ? */
			} else if (len > 1) {
				if (argv[i][0] == '-') {
					if (argv[i][1] == '-') {
						/* long options */
						for (idx = 0; options[idx].long_name; idx++) {
							if (strcmp(argv[i]+2, options[idx].long_name) == 0) {
								if (options[idx].need_arg) {
									if (i + 1 < argc) {
										argument = argv[i + 1];
										i++;
										goto handle_opt;
									} else {
										dprintf(fd, "'%s' needs an argument\n", argv[i]);
										goto failure;
									}
								}
								goto handle_opt;
							} else {
								size_t slen = strlen(options[idx].long_name);
								if (strncmp(argv[i]+2, options[idx].long_name, slen) == 0) {
									if (slen <= len + 2 && argv[i][2 + slen] == '=') {
										if (!options[idx].need_arg) {
											dprintf(fd, "flag '--%s' does not accept any arguments'\n", options[idx].long_name);
											goto failure;
										}
										argument = argv[i] + 3 + slen;
										goto handle_opt;
									}
								}
							}
						}
						dprintf(fd, "unknown flag '%s'\n", argv[i]);
						goto failure;
					} else {
						/* short options */
						shortflag = 1;

						for (size_t n = 1; n < len; n++) {
							for (idx = 0; options[idx].long_name; idx++) {
								if (options[idx].short_name && options[idx].short_name == argv[i][n]) {
									if (options[idx].need_arg) {
										if (n != len - 1) {
											dprintf(fd, "needed argument, got trailing flags after '%c' in '%s'\n", argv[i][n], argv[i]);
											goto failure;
										}
										if (i == argc - 1) {
											dprintf(fd, "'-%c' needs an argument\n", argv[i][n]);
											goto failure;
										}

										argument = argv[i + 1];
										i++;
									}
									goto handle_opt;
								}
							}

							dprintf(fd, "unknown flag '-%c'\n", argv[i][n]);
							goto failure;

							next_flag: {
								continue;
							}
						}

						shortflag = 0;
					}
				} else {
					paramv[paramc++] = argv[i];
					continue;
				}
			}
		}

		continue;

		handle_opt: {
			switch (options[idx].id) {
				case 1: /* match-suite */
					smatch = argument; break;
				case 2: /* match-test */
					tmatch = argument; break;
				case 3: /* verbose */
					verbose ++; break;
				case 4: /* quiet */
					verbose = 0; break;
				case 5: /* color */
					color = 1; break;
				case 6: /* no-color */
					color = 0; break;
				case 7: /* descriptor */
					fd = atoi(argument); break;
				case 8: /* list */
					list = 1; break;
				case 9: /* help */
				case 10: /* usage */
					help = 1; break;
				case 11: /* debugger */
					debugger = argument; break;
				case 12: /* bexec */
					bexec = strdup(argument); break;
				default:
					goto failure;
			}

			if (shortflag) {
				goto next_flag;
			}
			continue;
		}
		failure: {
			goto finalize;
		}
	}

	if (help || paramc == 0) {
		usage(fd);
		goto finalize;
	}

	paramv[paramc] = NULL;
	
	if (verbose)
		dprintf(fd, "#############################\n");
	dprintf(fd, "### The %sBUTCHER%s unit test ###\n",
			color?"\x1b[1;31m":"", color?"\x1b[0m":"");
	if (verbose) {
		dprintf(fd, "#############################\n\n");
		dprintf(fd, "\n");
	}

	if (smatch || tmatch)
		dprintf(fd, "tests matching '%s' in suites matching '%s' are going to be loaded\n\n",
				tmatch?tmatch:".*", smatch?smatch:".*");

	if (!bexec) {
		size_t plen = 0;
		char * path = NULL;
		path = strdup(argv[0]);
		if (!path) {
			err = ENOMEM;
			goto finalize;
		}
		plen = strlen(path) + 32 +1;
		bexec = malloc(plen);
		if (!bexec) {
			free(path);
			err = ENOMEM;
			goto finalize;
		}

		snprintf(bexec, plen, "%s/bexec", dirname(path));
		free(path);
	}

	err = bt_init(butcher, bexec, fd, smatch, tmatch);
	if (err)
		goto finalize;

	free(bexec);
	bexec = NULL;
	
	if (debugger) {
		err = bt_debugger(butcher, debugger);
		if (err)
			goto finalize;
	}

	err = bt_tune(butcher,
				((verbose>=1) ? BT_FLAG_VERBOSE:0) |
				((verbose>=2) ? BT_FLAG_DESCRIPTIONS:0) |
				((verbose>=3) ? BT_FLAG_MESSAGES:0) |
				(color ? BT_FLAG_COLOR:0)
			);
	if (err)
		goto finalize;

	err = bt_loadv(butcher, paramc, paramv);
	if (err) {
		dprintf(fd, "could not load one of shared objects in:\n");
		for (int i = 0; i < paramc; i++)
			dprintf(fd, "	'%s'\n", paramv[i]);
		dprintf(fd, "the last error was: %d\n", err);
		goto finalize;
	}

	if (!err) {
		if (list) {
			err = bt_list(butcher);
			if (err)
				goto finalize;
		} else {
			err = bt_chop(butcher);
			if (err)
				goto finalize;
			err = bt_report(butcher);
			if (err)
				goto finalize;
		}
	}

	finalize: {
		if (bexec)
			free(bexec);
		bt_delete(&butcher);
		exit(err);
	}
}
