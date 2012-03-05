#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include <dlfcn.h>

#include "bt-private.h"

#ifdef HAVE_LIBUNWIND
# define UNW_LOCAL_ONLY
# include <libunwind.h>
#endif

#include <pthread.h>

bt_tester_t tester;

void bt_backtrace()
{
#ifdef HAVE_LIBUNWIND
  unw_cursor_t cursor; unw_context_t uc;
  unw_word_t ip, sp, off;
  char buf[512];

  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);
    unw_get_proc_name(&cursor, buf, 512, &off);
    if (strcmp(buf, "main") == 0)
      return;
    printf ("in [0x%016lx] @ %s() + %ld\n", (long) ip, buf, (long) off);
  }
#endif
}

static inline
int get_env_bool(const char * name, int def)
{
  int ret = def;

  if (getenv(name)) {
    if (strcmp(getenv(name), "true") == 0)
      ret = 1;
    if (strcmp(getenv(name), "TRUE") == 0)
      ret = 1;
    if (strcmp(getenv(name), "1") == 0)
      ret = 1;
    if (strcmp(getenv(name), "false") == 0)
      ret = 0;
    if (strcmp(getenv(name), "FALSE") == 0)
      ret = 0;
    if (strcmp(getenv(name), "0") == 0)
      ret = 0;
  }

  return ret;
}

int main(int argc, char * argv[], char * env[])
{
  void * dl_handle;
  void * object;
  int    result, test_result, verbose, envdump, unload, wres;

  UNUSED_PARAM(argc);
  UNUSED_PARAM(argv);

  verbose = get_env_bool("butcher_verbose", 0);
  envdump = get_env_bool("butcher_envdump", 0);
  unload = get_env_bool("butcher_unload", 1);

  if (envdump) {
    fprintf(stderr, "BEXEC here ( env -i ");
    for (int i = 0; env[i]; i++)
      fprintf(stderr, "'%s' ", env[i]);
    for (int i = 0; argv[i]; i++)
      fprintf(stderr, "'%s' ", argv[i]);
    fprintf(stderr, ")\n");
  }

  char * dl_lib = getenv("butcher_elf_name");
  char * dl_setup = getenv("butcher_test_setup");
  char * dl_teardown = getenv("butcher_test_teardown");
  char * dl_test = getenv("butcher_test_function");
  char * cfd = getenv("butcher_cfd");

  if (!dl_lib) {
    fprintf(stderr, "butcher_elf_name not set\n");
    exit(-1);
  }
  if (!dl_test) {
    fprintf(stderr, "butcher_test_function not set\n");
    exit(-1);
  }

  if ((dl_handle = dlopen(dl_lib, RTLD_NOW)) == NULL) {
    fprintf(stderr, "ERROR: dlopen returned %s\n", dlerror());
    exit(-1);
  }

  const bt_fn_t * bsect;
  if (!(bsect = dlsym(dl_handle, "__start_bexec"))) {
    fprintf(stderr, "ERROR: no bexec section: %s\n", dlerror());
    exit(-1);
  }

  const bt_fn_t * bsect_end;
  if (!(bsect_end = dlsym(dl_handle, "__stop_bexec"))) {
    fprintf(stderr, "ERROR: no valid bexec section: %s\n", dlerror());
    exit(-1);
  }

  FILE * oldstdout = NULL;

  if (cfd) {
    tester.cfd = atoi(cfd);
  } else {
    tester.cfd  = -1;
    oldstdout = stdout;
    stdout = stderr;
  }

  if (dl_setup) {
    unsigned id = atol(dl_setup);
    if (bsect + id < bsect_end) {
      tester.setup = bsect[id].function;
    } else {
      fprintf(stderr, "ERROR: invalid setup function: %u\n", id);
      exit(-1);
    }
  } else {
    tester.setup = NULL;
  }

  if (dl_teardown) {
    unsigned id = atol(dl_teardown);
    if (bsect + id < bsect_end) {
      tester.teardown = bsect[id].function;
    } else {
      fprintf(stderr, "ERROR: invalid setup function: %u\n", id);
      exit(-1);
    }
  } else {
    tester.teardown = NULL;
  }

  {
    unsigned id = atol(dl_test);
    if (bsect + id < bsect_end) {
      tester.function = bsect[id].function;
    } else {
      fprintf(stderr, "ERROR: invalid setup function: %u\n", id);
      exit(-1);
    }
  }

  /* stdout will be redirected */
  tester.fd = STDOUT_FILENO;
  close(STDIN_FILENO);

  wres = get_env_bool("butcher_wres", 1);

  /* do not write control strings to terminal */
  if (wres && isatty(tester.fd))
    wres = 0;

  struct result_rec rec = {
    "\x01\x02\x03\x04",
    {BT_TEST_NONE, BT_TEST_NONE, BT_TEST_NONE},
    0
  };

  result = BT_TEST_NONE;

  if (tester.setup) {
    result = (*tester.setup)(NULL, &object);
    if (result == BT_RESULT_OK)
      rec.results[BT_PASS_SETUP] = BT_TEST_SUCCEEDED;
    else if (result == BT_RESULT_IGNORE)
      rec.results[BT_PASS_SETUP] = BT_TEST_IGNORED;
    else if (result == BT_RESULT_FAIL)
      rec.results[BT_PASS_SETUP] = BT_TEST_FAILED;
  } else {
    rec.results[BT_PASS_SETUP] = BT_TEST_NONE;
  }

  if (tester.cfd != -1) {
    write(tester.cfd, &rec, sizeof(struct result_rec));
  }

  /* does not make much sense to run the test if setup has failed */
  if (result <= BT_TEST_SUCCEEDED) {
    test_result = (*tester.function)(object, &object);

    if (test_result == BT_RESULT_OK)
      rec.results[BT_PASS_TEST] = BT_TEST_SUCCEEDED;
    else if (test_result == BT_RESULT_IGNORE)
      rec.results[BT_PASS_TEST] = BT_TEST_IGNORED;
    else if (test_result == BT_RESULT_FAIL)
      rec.results[BT_PASS_TEST] = BT_TEST_FAILED;
    else
      rec.results[BT_PASS_TEST] = BT_TEST_CORRUPTED;

    if (tester.cfd != -1) {
      write(tester.cfd, &rec, sizeof(struct result_rec));
    }
  }

  if (result <= BT_TEST_SUCCEEDED) {
    if (tester.teardown) {
      result = (*tester.teardown)(object, &object);
      if (result == BT_RESULT_OK)
        rec.results[BT_PASS_TEARDOWN] = BT_TEST_SUCCEEDED;
      else if (result == BT_RESULT_IGNORE)
        rec.results[BT_PASS_TEARDOWN] = BT_TEST_IGNORED;
      else if (result == BT_RESULT_FAIL)
        rec.results[BT_PASS_TEARDOWN] = BT_TEST_FAILED;

      if (tester.cfd != -1) {
        write(tester.cfd, &rec, sizeof(struct result_rec));
      }
    }
  }

  /* we are done */
  rec.done = 1;

  fflush(stdout);
  fflush(stderr);

  if (tester.cfd != -1) {
    write(tester.cfd, &rec, sizeof(struct result_rec));
  }

  close(tester.fd);

  if (unload)
    dlclose(dl_handle);

  if (oldstdout) {
    stdout = oldstdout;
  }

  pthread_exit(NULL);
}
