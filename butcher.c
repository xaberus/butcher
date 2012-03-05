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

enum butcher_opt {
  OPT_ERROR = 0,
  OPT_MATCH_SUITE,
  OPT_MATCH_TEST,
  OPT_VERBOSE,
  OPT_QUIET,
  OPT_COLOR,
  OPT_NOCOLOR,
  OPT_DESCRIPTOR,
  OPT_LIST,
  OPT_HELP,
  OPT_USAGE,
  OPT_BEXEC,
  OPT_DEBUGGER,
  OPT_VALGRIND,
  OPT_CGDB,
  OPT_GDB,
};

static const struct options {
  enum butcher_opt id;
  const char * long_name;
  char short_name;
  char need_arg;
  const char * help;
} options[] = {
  {OPT_MATCH_SUITE,
    .long_name = "match-suite",
    .short_name = 's', .need_arg = 1,
    .help = "run only tests in matched suites; <arg> is a regex\n"
      "as povided by the POSIX2 specification (man 7 regex)"
  },
  {OPT_MATCH_TEST,
    .long_name = "match-test",
    .short_name = 't', .need_arg = 1,
    .help = "run only matched tests; <arg> is a regex"
  },
  {OPT_VERBOSE,
    .long_name = "verbose",
    .short_name = 'v', .need_arg = 0,
    .help = "be verbose, repeat for descriptions and messages (in order)"
  },
  {OPT_QUIET,
    .long_name = "quiet",
    .short_name = 'q', .need_arg = 0,
    .help = "be quiet (default)"
  },
  {OPT_COLOR,
    .long_name = "color",
    .short_name = 'c', .need_arg = 0,
    .help = "enable color output"
  },
  {OPT_NOCOLOR,
    .long_name = "no-color",
    .short_name = 'n', .need_arg = 0,
    .help = "disable color output"
  },
  {OPT_DESCRIPTOR,
    .long_name = "descriptor",
    .short_name = 'd', .need_arg = 1,
    .help = "write to given descriptor instead of 1 (stdout)"
  },
  {OPT_LIST,
    .long_name = "list",
    .short_name = 'l', .need_arg = 0,
    .help = "instead of running tests, just dump everything available"
  },
  {OPT_HELP,
    .long_name = "help",
    .short_name = 'h', .need_arg = 0,
    .help = "display this screen"
  },
  {OPT_USAGE,
    .long_name = "usage",
    .short_name = 0, .need_arg = 0,
    .help = "display this screen"
  },
  {OPT_BEXEC,
    .long_name = "bexec",
    .short_name = 'b', .need_arg = 1,
    .help = "use <arg> as path to bexec, e.g. /usr/bin/bexec"
  },
  {OPT_DEBUGGER,
    .long_name = "debugger",
    .short_name = 'g', .need_arg = 1,
    .help = "use <arg> as debugger for bexec, e.g. /usr/bin/valgrind"
  },
  {OPT_VALGRIND,
    .long_name = "--valgrind",
    .short_name = 'V', .need_arg = 0,
    .help = "equivalent of -g 'valgrind --show-reachable=yes --leak-check=full'"
  },
  {OPT_CGDB,
    .long_name = "--cgdb",
    .short_name = 'C', .need_arg = 0,
    .help = "equivalent of -g 'cgdb'"
  },
  {OPT_GDB,
    .long_name = "--gdb",
    .short_name = 'G', .need_arg = 0,
    .help = "equivalent of -g 'gdb'"
  },
  {OPT_ERROR, NULL, 0, 0, NULL}
};

void usage(FILE * fd)
{
  unsigned int idx;

  fprintf(fd,
      "The BUTCHER unit test - runs test functions inside shared objects \n"
      "with the help of libelf and libdl. See \"bt.h\" for details.\n"
      "\n"
      "Usage: \n"
      "	export LD_LIBRARY_PATH=<path to link dependencies>\n"
      "	butcher <options> <shared-objects>\n");
  fprintf(fd,
      "Options: \n");

  for (idx = 0; options[idx].long_name; idx++) {
    if (options[idx].short_name)
      fprintf(fd,
          "	--%s%s (-%c%s)\n",
          options[idx].long_name, options[idx].need_arg ? " <arg>" : "",
          options[idx].short_name, options[idx].need_arg ? " <arg>" : "");
    else
      fprintf(fd,
          "	--%s %s\n", options[idx].long_name, options[idx].need_arg ? "<arg>" : "");

    const char * s = options[idx].help;
    const char * e = NULL;

    while (s && *s) {
      e = strchr(s, '\n');
      if (e) {
        fprintf(fd,
            "		%.*s\n", (int) (e - s), s);
      } else {
        fprintf(fd,
            "		%s\n", s);
      }
      s = (e) ? e + 1 : NULL;
    }
    ;
  }

  fprintf(fd,
      "Example: \n"
      "	butcher -cv -s '^ugly' -t '^important' libfoo.so libbar.so\n");
}

int main(int argc, char * argv[], char * env[])
{

  bt_t       * butcher = NULL;
  int          err;
  int          paramc;
  char       * paramv[argc + 1];
  int          i, shortflag;
  size_t       len;
  char       * smatch, * tmatch;
  int          list, help, verbose, color;
  unsigned int idx;
  char       * argument, * bexec, * debugger;
  FILE       * fd = NULL;
  int          ofd = STDOUT_FILENO;

  UNUSED_PARAM(env);

  err = bt_new(&butcher);
  if (err)
    return_error(err);

  smatch = NULL;
  tmatch = NULL;
  list = 0;
  help = 0;
  verbose = 0;
  color = 1;
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
              if (strcmp(argv[i] + 2, options[idx].long_name) == 0) {
                if (options[idx].need_arg) {
                  if (i + 1 < argc) {
                    argument = argv[i + 1];
                    i++;
                    goto handle_opt;
                  } else {
                    fprintf(stderr, "'%s' needs an argument\n", argv[i]);
                    goto failure;
                  }
                }
                goto handle_opt;
              } else {
                size_t slen = strlen(options[idx].long_name);
                if (strncmp(argv[i] + 2, options[idx].long_name, slen) == 0) {
                  if (slen <= len + 2 && argv[i][2 + slen] == '=') {
                    if (!options[idx].need_arg) {
                      fprintf(stderr,
                          "flag '--%s' does not accept any arguments'\n",
                          options[idx].long_name);
                      goto failure;
                    }
                    argument = argv[i] + 3 + slen;
                    goto handle_opt;
                  }
                }
              }
            }
            fprintf(stderr, "unknown flag '%s'\n", argv[i]);
            goto failure;
          } else {
            /* short options */
            shortflag = 1;

            for (size_t n = 1; n < len; n++) {
              for (idx = 0; options[idx].long_name; idx++) {
                if (options[idx].short_name && options[idx].short_name == argv[i][n]) {
                  if (options[idx].need_arg) {
                    if (n != len - 1) {
                      fprintf(stderr, "needed argument, got trailing flags after '%c' in '%s'\n",
                          argv[i][n], argv[i]);
                      goto failure;
                    }
                    if (i == argc - 1) {
                      fprintf(stderr, "'-%c' needs an argument\n", argv[i][n]);
                      goto failure;
                    }

                    argument = argv[i + 1];
                    i++;
                  }
                  goto handle_opt;
                }
              }

              fprintf(stderr, "unknown flag '-%c'\n", argv[i][n]);
              goto failure;
next_flag:
              continue;
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
        case OPT_MATCH_SUITE:
          smatch = argument; break;
        case OPT_MATCH_TEST:
          tmatch = argument; break;
        case OPT_VERBOSE:
          verbose++; break;
        case OPT_QUIET:
          verbose = 0; break;
        case OPT_COLOR:
          color = 1; break;
        case OPT_NOCOLOR:
          color = 0; break;
        case OPT_DESCRIPTOR:
          ofd = atoi(argument); break;
        case OPT_LIST:
          list = 1; break;
        case OPT_HELP:
        case OPT_USAGE:
          help = 1; break;
        case OPT_DEBUGGER:
          debugger = argument; break;
        case OPT_BEXEC:
          bexec = strdup(argument); break;
        case OPT_VALGRIND:
          debugger = "valgrind --show-reachable=yes --leak-check=full"; break;
        case OPT_CGDB:
          debugger = "cgdb"; break;
        case OPT_GDB:
          debugger = "gdb"; break;
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

  fd = fdopen(ofd, "w");
  if (!fd) {
    fprintf(stderr, "could not open passed fd\n");
    goto finalize;
  }

  if (help || paramc == 0) {
    usage(fd);
    goto finalize;
  }

  paramv[paramc] = NULL;

  if (!debugger) {
    if (verbose)
      fprintf(fd, "#############################\n");
    fprintf(fd, "### The %sBUTCHER%s unit test ###\n",
        color ? "\x1b[1;31m" : "", color ? "\x1b[0m" : "");
    if (verbose) {
      fprintf(fd, "#############################\n\n");
      fprintf(fd, "\n");
    }
  }

  if (debugger) {
    if (!tmatch)
      tmatch = "[^A-Za-z0-9_]\\+";
  }

  if (smatch || tmatch)
    fprintf(fd, "tests matching '%s' in suites matching '%s' are going to be loaded\n",
        tmatch ? tmatch : ".*", smatch ? smatch : ".*");

  if (!bexec) {
    size_t plen = 0;
    char * path = NULL;
    path = strdup(argv[0]);
    if (!path) {
      err = ENOMEM;
      goto finalize;
    }
    plen = strlen(path) + 32 + 1;
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
      ((verbose>=1) ? BT_FLAG_VERBOSE : 0) |
      ((verbose>=2) ? BT_FLAG_DESCRIPTIONS : 0) |
      ((verbose>=3) ? BT_FLAG_MESSAGES : 0) |
      ((verbose>=4) ? BT_FLAG_ENVDUMP : 0) |
      (color ? BT_FLAG_COLOR : 0)
               );
  if (err)
    goto finalize;

  err = bt_loadv(butcher, paramc, paramv);
  if (err) {
    fprintf(fd, "could not load one of shared objects in:\n");
    for (int i = 0; i < paramc; i++)
      fprintf(fd, "	'%s'\n", paramv[i]);
    fprintf(fd, "the last error was: %d\n", err);
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
      if (!debugger) {
        err = bt_report(butcher);
        if (err)
          goto finalize;
      }
    }
  }

finalize: {
    if (bexec)
      free(bexec);
    bt_delete(&butcher);
    if (fd)
      fclose(fd);
    exit(err);
  }
}
