#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include <dlfcn.h>

#include "bt-private.h"

#include <pthread.h>

bt_tester_t tester;

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
  int    result, test_result, verbose, envdump, unload;

  UNUSED_PARAM(argc);
  UNUSED_PARAM(argv);

  verbose = get_env_bool("butcher_verbose", 0);
  envdump = get_env_bool("butcher_envdump", 0);
  unload = get_env_bool("butcher_unload", 1);

#if 0
  if (argc != 5) {
    dprintf(STDERR_FILENO, "argc: %d != 5!\n", argc);
    exit(-1);
  }
#endif

  if (envdump) {
    dprintf(STDERR_FILENO, "BEXEC here ( env -i ");
    for (int i = 0; env[i]; i++)
      dprintf(STDERR_FILENO, "'%s' ", env[i]);
    for (int i = 0; argv[i]; i++)
      dprintf(STDERR_FILENO, "'%s' ", argv[i]);
    dprintf(STDERR_FILENO, ")\n");
  }

  char * dl_lib = getenv("butcher_elf_name");
  char * dl_setup = getenv("butcher_suite_setup");
  char * dl_teardown = getenv("butcher_suite_teardown");
  char * dl_test = getenv("butcher_test_function");

  if (!dl_lib) {
    dprintf(STDERR_FILENO, "butcher_elf_name not set\n");
    exit(-1);
  }
  if (!dl_test) {
    dprintf(STDERR_FILENO, "butcher_test_function not set\n");
    exit(-1);
  }

  if ((dl_handle = dlopen(dl_lib, RTLD_NOW)) == NULL) {
    dprintf(STDERR_FILENO, "ERROR: dlopen returned %s\n", dlerror());
    exit(-1);
  }

  void * ptr;

  if (dl_setup) {
    if ((ptr = dlsym(dl_handle, dl_setup)) == NULL)
      dlerror();
    *(void **) &tester.setup = ptr;
  } else {
    tester.setup = NULL;
  }

  if (dl_teardown) {
    if ((ptr = dlsym(dl_handle, dl_teardown)) == NULL)
      dlerror();
    *(void **) &tester.teardown = ptr;
  } else {
    tester.teardown = NULL;
  }

  if ((ptr = dlsym(dl_handle, dl_test)) == NULL) {
    dlerror();
    exit(-1);
  }
  *(void **) &tester.function = ptr;

  /* stdout will be redirected */
  tester.fd = STDOUT_FILENO;
  close(STDIN_FILENO);

  struct result_rec rec = {
    "\x01\x02\x03\x04",
    {BT_TEST_NONE, BT_TEST_NONE, BT_TEST_NONE},
    0
  };

  result = BT_TEST_NONE;

  if (tester.setup) {
    result = (*tester.setup)(&object);
    if (result == BT_RESULT_OK)
      rec.results[BT_PASS_SETUP] = BT_TEST_SUCCEEDED;
    else if (result == BT_RESULT_IGNORE)
      rec.results[BT_PASS_SETUP] = BT_TEST_IGNORED;
    else if (result == BT_RESULT_FAIL)
      rec.results[BT_PASS_SETUP] = BT_TEST_FAILED;
  } else {
    rec.results[BT_PASS_SETUP] = BT_TEST_NONE;
  }

  write(tester.fd, "\0", 1);
  write(tester.fd, &rec, sizeof(struct result_rec));

  /* does not make much sense to run the test if setup has failed */
  if (result <= BT_TEST_SUCCEEDED) {
    test_result = (*tester.function)(object);

    if (test_result == BT_RESULT_OK)
      rec.results[BT_PASS_TEST] = BT_TEST_SUCCEEDED;
    else if (test_result == BT_RESULT_IGNORE)
      rec.results[BT_PASS_TEST] = BT_TEST_IGNORED;
    else if (test_result == BT_RESULT_FAIL)
      rec.results[BT_PASS_TEST] = BT_TEST_FAILED;
    else
      rec.results[BT_PASS_TEST] = BT_TEST_CORRUPTED;

    write(tester.fd, "\0", 1);
    write(tester.fd, &rec, sizeof(struct result_rec));
  }

  if (result <= BT_TEST_SUCCEEDED) {
    if (tester.teardown) {
      result = (*tester.teardown)(&object);
      if (result == BT_RESULT_OK)
        rec.results[BT_PASS_TEARDOWN] = BT_TEST_SUCCEEDED;
      else if (result == BT_RESULT_IGNORE)
        rec.results[BT_PASS_TEARDOWN] = BT_TEST_IGNORED;
      else if (result == BT_RESULT_FAIL)
        rec.results[BT_PASS_TEARDOWN] = BT_TEST_FAILED;

      write(tester.fd, "\0", 1);
      write(tester.fd, &rec, sizeof(struct result_rec));
    }
  }

  /* we are done */
  rec.done = 1;

  write(tester.fd, "\0", 1);
  write(tester.fd, &rec, sizeof(struct result_rec));

  close(tester.fd);

  if (unload)
    dlclose(dl_handle);
  pthread_exit(NULL);
}
