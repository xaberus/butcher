/* @@SOURCE-HEADER@@ */

#include "bt-private.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include <fcntl.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

/*************************************************/

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[1;34m"
#define PURPLE "\033[1;35m"
#define CYAN "\033[1;36m"

#define RED_BG "\033[2;41m"
#define CYAN_BG "\033[2;46m"

#define ENDCOL "\033[0m"

#define _hash_rot(x, k) \
  (((x) << (k)) | ((x) >> (32 - (k))))

#define _hash_mix(a, b, c) \
  { \
    a -= c;  a ^= _hash_rot(c, 4);  c += b; \
    b -= a;  b ^= _hash_rot(a, 6);  a += c; \
    c -= b;  c ^= _hash_rot(b, 8);  b += a; \
    a -= c;  a ^= _hash_rot(c, 16);  c += b; \
    b -= a;  b ^= _hash_rot(a, 19);  a += c; \
    c -= b;  c ^= _hash_rot(b, 4);  b += a; \
  }

#define _hash_final(a, b, c) \
  { \
    c ^= b; c -= _hash_rot(b, 14); \
    a ^= c; a -= _hash_rot(c, 11); \
    b ^= a; b -= _hash_rot(a, 25); \
    c ^= b; c -= _hash_rot(b, 16); \
    a ^= c; a -= _hash_rot(c, 4); \
    b ^= a; b -= _hash_rot(a, 14); \
    c ^= b; c -= _hash_rot(b, 24); \
  }

/**
 * hashes a string
 *
 * @param[in] key the buffer to hash
 * @param[in] length the size of buffer
 * @param[in] initval hash salt
 *
 * @return a hash value
 */

inline static
unsigned hash(const void * key, size_t length, unsigned initval)
{
  unsigned a, b, c;

  a = b = c = 0xdeadbeef + ((unsigned) length) + initval;

  {
    const unsigned * k = (const unsigned *) key;

    while (length > 12) {
      a += k[0];
      b += k[1];
      c += k[2];
      _hash_mix(a, b, c);
      length -= 12;
      k += 3;
    }
    if (length > 0) {
      unsigned buff[3] = {0};
      memcpy(buff, k, length);
      a += buff[0];
      b += buff[1];
      c += buff[2];
      _hash_mix(a, b, c);
      _hash_final(a, b, c);
    }
  }

  return c;
}

#define BT_HASH_SALT 777

/* this ias a stup to we can load test using it */
void bt_backtrace()
{

}

/**
 * creates a new log line
 *
 * @param[out] line a pointer to a pointer to hold the line
 * @param[in] msg a message the line to contain or NULL to allocate a line to hold
 *        a message of size len
 * @param[in] len the size of msg or -1, in which case strlen() is used to get this information
 *        (if msg is NULL len must be > 0 or an error will be generated
 *
 * @return the operation error code
 */

int bt_log_line_new(bt_log_line_t ** line, const char * msg, ssize_t len)
{
  bt_log_line_t * self;

  if (!line)
    return_error(EINVAL);

  if (len < 0 && msg)
    len = strlen(msg);

  if (len < 0)
    return_error(EINVAL);

  self = malloc(sizeof(bt_log_line_t) + len + 1);
  if (!self)
    return_error(ENOMEM);

  memset(self, 0, sizeof(bt_log_line_t));

  if (msg) {
    memcpy(self->contents, msg, len);
    self->contents[len] = '\0';
  } else {
    self->contents[0] = '\0';
  }

  *line = self;

  return 0;
}
/**
 * deletes a log line
 *
 * @param[in] line a pointer to a pointer holding the line (i.e not NULL)
 *
 * @return the operation error code
 */

int bt_log_line_delete(bt_log_line_t ** line)
{
  if (!line || !*line)
    return_error(EINVAL);

  free(*line);

  return 0;
}

/**
 * creates a new log
 *
 * @param[out] log a pointer to a pointer to hold the log
 *
 * @return the operation error code
 */

int bt_log_new(bt_log_t ** log)
{
  bt_log_t * self;

  if (!log)
    return_error(EINVAL);

  self = malloc(sizeof(bt_log_t));
  if (!self)
    return_error(ENOMEM);

  memset(self, 0, sizeof(bt_log_t));

  self->lines = NULL;
  self->last = NULL;

  *log = self;

  return 0;
}

#if 0
/**
 * appends a log line to the log
 *
 * @param[in] self a pointer holding the log
 * @param[in] line a lo line to append to the log
 *
 * @return the operation error code
 */

int bt_log_append(bt_log_t * self, bt_log_line_t * line)
{
  if (!self || !line)
    return_error(EINVAL);

  if (self->last) {
    self->last->next = line;
    self->last = line;
  } else {
    self->lines = line;
    self->last = line;
  }

  return 0;
}
#endif

/**
 * appends a log line to the log, which is stored in a char buffer
 *
 * @param[in] self a pointer holding the log
 * @param[in] msg a message the line to contain
 * @param[in] len the size of msg or -1, in which case strlen() is used to get this information
 *
 * @return the operation error code
 */

int bt_log_msgcpy(bt_log_t * self, const char * msg, ssize_t len)
{
  bt_log_line_t * line;
  int err;

  if (!self || !msg)
    return_error(EINVAL);

  err = bt_log_line_new(&line, msg, len);
  if (err)
    return_error(err);

  if (self->last) {
    self->last->next = line;
    self->last = line;
  } else {
    self->lines = line;
    self->last = line;
  }

  return 0;
}

/**
 * deletes a log
 *
 * @param[in] log a pointer to a pointer holding the log
 *
 * @return the operation error code
 */

int bt_log_delete(bt_log_t ** log)
{
  bt_log_t * self;

  if (!log || !*log)
    return_error(EINVAL);

  self = *log;

  bt_log_line_t * cur, * tmp;
  cur = self->lines;
  while (cur) {
    tmp = cur->next;
    bt_log_line_delete(&cur);
    cur = tmp;
  }

  free(self);

  return 0;
}

/**
 * deletes a test not touching anything else
 *
 * @param[in] test a pointer to a pointer holding the test
 *
 * @return the operation error code
 */

int bt_test_delete(bt_test_t ** test)
{
  bt_test_t * self;

  if (!test || !*test)
    return_error(EINVAL);

  self = *test;

  free(self->name);
  if (self->log)
    bt_log_delete(&self->log);

  free(self);

  return 0;
}


/**
 * creates a new test for a given suite which is is intended to run function
 * indicated by dlname
 *
 * @param[out] test a pointer to a pointer to hold the test
 * @param[in] id function if of the test
 * @param[in] kind kind of function to call
 * @param[in] name name of the test
 *
 * @return the operation error code
 */

static
int bt_test_new(bt_test_t ** test, unsigned id, bt_fn_kind_t kind, const char * name)
{
  bt_test_t * self;
  int err;


  if (!test)
    return_error(EINVAL);

  self = malloc(sizeof(bt_test_t));
  if (!self)
    return_error(ENOMEM);

  memset(self, 0, sizeof(bt_test_t));

  self->next = NULL;
  memset(self->results, BT_TEST_NONE, BT_PASS_MAX);
  self->log = NULL;

  self->id = id;
  self->kind = kind;

  self->name = strdup(name);
  if (!self->name) {
    err = ENOMEM;
    goto failure;
  }

  self->setupid = BT_NO_ID;
  self->teardownid = BT_NO_ID;

  *test = self;

  return 0;

failure:
  bt_test_delete(&self);
  return_error(err);
}

/**
 * recursively deletes a test suite (i.e. the suite and its tests, logs, results)
 *
 * @param[in] suite a pointer to a pointer holding the suite
 *
 * @return the operation error code
 */

int bt_suite_delete(bt_suite_t ** suite)
{
  bt_suite_t * self;
  bt_test_t * cur, * tmp;
  unsigned n;

  if (!suite || !*suite)
    return_error(EINVAL);

  self = *suite;

  for (n = 0; n <self->hsize; n++) {
    cur = self->htests[n];
    while (cur) {
      tmp = cur->next;
      bt_test_delete(&cur);
      cur = tmp;
    }
  }
  free(self->htests);

  free(self->name);

  free(self);

  return 0;
}

/**
 * creates an empty suite
 *
 * @param[out] suite a pointer to a pointer to hold the suite
 * @param[in] name an unique name for the suite
 *
 * @return the operation error code
 */

static
int bt_suite_new(bt_suite_t ** suite, const char * name)
{
  bt_suite_t * self;
  int err;

  if (!suite)
    return_error(EINVAL);

  self = malloc(sizeof(bt_suite_t));
  if (!self)
    return_error(ENOMEM);

  memset(self, 0, sizeof(bt_suite_t));

  self->name = strdup(name);
  if (!self->name) {
    err = ENOMEM;
    goto failure;
  }

  self->htests = malloc(sizeof(bt_test_t *) * 128);
  if (!self->htests) {
    err = ENOMEM;
    goto failure;
  }
  memset(self->htests, 0, sizeof(bt_test_t *) * 128);
  self->hsize = 128;

  *suite = self;

  return 0;

failure:
  bt_suite_delete(&self);
  return_error(err);
}

/**
 * adds a teat to a suite using test->name as key
 *
 * @param[in] self the suite to add the test to
 * @param[in] suite the test to add
 *
 * @return the operation error code
 */

int bt_suite_add_test(bt_suite_t * self, bt_test_t * test)
{
  unsigned h, idx;

  h = hash(test->name, strlen(test->name), BT_HASH_SALT);
  idx = h % self->hsize;
  test->next = self->htests[idx];
  self->htests[idx] = test;

  return 0;
}


/**
 * gets a suite form the elf hashmap
 *
 * @param[in] elf a pointer to an elf
 * @param[in] name name of the suite to get
 *
 * @return the suite of NULL
 */

bt_test_t * bt_suite_get_test(bt_suite_t * self, const char * name)
{
  unsigned h, idx;
  bt_test_t * test;

  h = hash(name, strlen(name), BT_HASH_SALT);
  idx = h % self->hsize;
  test = self->htests[idx];
  while (test) {
    if (strcmp(test->name, name) == 0)
      return test;

    test = test->next;
  }

  return NULL;
}

/**
 * recursively deletes an elf descriptor (i.e. elf, test suites, tests, logs, results)
 *
 * @param[in] elf a pointer to a pointer holding the elf
 *
 * @return the operation error code
 */

int bt_elf_delete(bt_elf_t ** elf)
{
  bt_elf_t * self;
  bt_suite_t * cur, * tmp;
  unsigned n;

  if (!elf || !*elf)
    return_error(EINVAL);

  self = *elf;

  for (n = 0; n <self->hsize; n++) {
    cur = self->hsuites[n];
    while (cur) {
      tmp = cur->next;
      bt_suite_delete(&cur);
      cur = tmp;
    }
  }
  free(self->hsuites);

  if (self->dlhandle)
    dlclose(self->dlhandle);

  free(self->name);

  free(self);

  return 0;
}

/**
 * creates a new descriptor for a shared object
 *
 * @param[out] elf a pointer to a pointer to hold the elf
 * @param[in] elfname filename of the shared object to pass to open() and dlopen()
 *
 * @return the operation error code
 */

int bt_elf_new(bt_elf_t ** elf, bt_t * butcher, const char * elfname)
{
  bt_elf_t * self;
  int err;

  if (!elf || !butcher || !elfname)
    return_error(EINVAL);

  self = malloc(sizeof(bt_elf_t));
  if (!self)
    return_error(ENOMEM);

  memset(self, 0, sizeof(bt_elf_t));

  self->next = NULL;

  self->butcher = butcher;

  self->name = strdup(elfname);
  if (!self->name) {
    err = ENOMEM;
    goto failure;
  }

  self->hsuites = malloc(sizeof(bt_suite_t *) * 128);
  if (!self->hsuites) {
    err = ENOMEM;
    goto failure;
  }
  memset(self->hsuites, 0, sizeof(bt_suite_t *) * 128);
  self->hsize = 128;

  self->dlhandle = dlopen(self->name, RTLD_NOW);
  if (!self->dlhandle) {
    fprintf(self->butcher->fd, "could not open shared object '%s': %s\n", elfname, dlerror());
    err = ENFILE;
    goto failure;
  }

  *elf = self;

  return 0;

failure: {
    bt_elf_delete(&self);
    return_error(err);
  }
}

/**
 * gets a suite form the elf hashmap
 *
 * @param[in] elf a pointer to an elf
 * @param[in] name name of the suite to get
 *
 * @return the suite of NULL
 */

bt_suite_t * bt_elf_get_suite(bt_elf_t * self, const char * name)
{
  unsigned h, idx;
  bt_suite_t * suite;

  h = hash(name, strlen(name), BT_HASH_SALT);
  idx = h % self->hsize;
  suite = self->hsuites[idx];
  while (suite) {
    if (strcmp(suite->name, name) == 0)
      return suite;

    suite = suite->next;
  }

  return NULL;
}

/**
 * adds a suite to an elf using suite->name as key
 *
 * @param[in] self the elf to add the suite to
 * @param[in] suite the suite to add
 *
 * @return the operation error code
 */

int bt_elf_add_suite(bt_elf_t * self, bt_suite_t * suite)
{
  unsigned h, idx;

  h = hash(suite->name, strlen(suite->name), BT_HASH_SALT);
  idx = h % self->hsize;
  bt_suite_t ** p = &self->hsuites[idx];
  bt_suite_t * cur = *p;
  while (cur) {
    if (strcmp(cur->name, suite->name) == 0)
      return_error(EINVAL);
    p = &cur->next;
    cur = *p;
  }

  suite->next = *p;
  *p = suite;

  return 0;
}


/**
 * assigns a setup function to an already registered test
 *
 * @param[in] self an elf to search for suite and test
 * @param[in] fn a function specifier
 *
 * @return the operation error code
 *
 * i.e. self[fn->extra][fn->name].setupid = id
 */

int bt_elf_assign_setup(bt_elf_t * self, unsigned id, const bt_fn_t * fn)
{
  bt_suite_t * suite;
  bt_test_t * test;

  suite = bt_elf_get_suite(self, fn->extra);
  if (suite) {
    test = bt_suite_get_test(suite, fn->name);
    if (test) {
      if (test->setupid == BT_NO_ID) {
        test->setupid = id;
        return 0;
      }
      fprintf(stderr, "attempted to redefine setup function for test %s\n", fn->name);
    }
  }

  return EINVAL;
}

/**
 * assigns a teardown function to an already registered test
 *
 * @param[in] self an elf to search for suite and test
 * @param[in] fn a function specifier
 *
 * @return the operation error code
 *
 * i.e. self[fn->extra][fn->name].teardownid = id
 */

int bt_elf_assign_teardown(bt_elf_t * self, unsigned id, const bt_fn_t * fn)
{
  bt_suite_t * suite;
  bt_test_t * test;

  suite = bt_elf_get_suite(self, fn->extra);
  if (suite) {
    test = bt_suite_get_test(suite, fn->name);
    if (test) {
      if (test->teardownid == BT_NO_ID) {
        test->teardownid = id;
        return 0;
      }
      fprintf(stderr, "attempted to redefine teardown function for test %s\n", fn->name);
    }
  }

  return EINVAL;
}

/**
 * registers a test in an elf creating suites as needed
 *
 * @param[in] self an elf to register the test
 * @param[in] id a function in
 * @param[in] kind kind the the function
 * @param[in] fn function descriptor
 *
 * @return the operation error code
 */

int bt_elf_register_test(bt_elf_t * self, unsigned id, bt_fn_kind_t kind, const bt_fn_t * fn)
{
  const char * sname;
  bt_suite_t * suite;
  bt_test_t * test;
  int err;

  sname = fn->extra ? fn->extra : "(nil)";
  suite = bt_elf_get_suite(self, sname);
  if (!suite) {
    err = bt_suite_new(&suite, sname);
    if (err)
      goto failure;

    err = bt_elf_add_suite(self, suite);
    if (err)
      goto failure;
  }

  err = bt_test_new(&test, id, kind, fn->name);
  if (err)
    goto failure;

  err = bt_suite_add_test(suite, test);
  if (err)
    goto failure;

 return 0;
failure:
    return_error(err);
}

/**
 * iterates the shared object and creates empty test suites from definitions
 *
 * @param[in] self  a pointer to an elf descriptor
 *
 * @return the operation error code
 */

int bt_elf_load2(bt_elf_t * self)
{
  const bt_fn_t * bsect;
  const bt_fn_t * bsect_end;
  const bt_fn_t * fn;
  unsigned fnid;
  int err = 0;

  if (!self)
    return_error(EINVAL);

  bsect = dlsym(self->dlhandle, "__start_bexec");
  if (!bsect) {
    fprintf(stderr, "shared object does not export a test section\n");
    err = ENFILE;
    goto failure;
  }
  bsect_end = dlsym(self->dlhandle, "__stop_bexec");
  if (!bsect_end) {
    fprintf(stderr, "shared object does not export a valid test section\n");
    err = ENFILE;
    goto failure;
  }

  /*unsigned long * p;
  for (p = bsect; p < bsect_end; p++) {
    fprintf(stderr, "### 0x%016x\n", *p);
  }*/

  fnid = 0;
  for (fn = bsect; fn < bsect_end; fn++, fnid++) {
    switch ((bt_fn_kind_t) (fn->flags & 0xf)) {
      case BT_FN_KIND_PTEST:
        err = bt_elf_register_test(self, fnid, BT_FN_KIND_PTEST, fn);
        break;
      case BT_FN_KIND_FTEST:
        err = bt_elf_register_test(self, fnid, BT_FN_KIND_FTEST, fn);
        break;
      default:
        continue;
    }
    if (err)
      goto failure;
  }

  fnid = 0;
  for (fn = bsect; fn < bsect_end; fn++, fnid++) {
    switch ((bt_fn_kind_t) (fn->flags & 0xf)) {
      case BT_FN_KIND_SETUP:
        err = bt_elf_assign_setup(self, fnid, fn);
        break;
      case BT_FN_KIND_TEARDOWN:
        err = bt_elf_assign_teardown(self, fnid, fn);
        break;
      default:
        continue;
    }
    if (err)
      goto failure;
  }

  return 0;

failure:
    return_error(err);
}

/**
 * creates a new butcher unit test evaluator object
 *
 * @param[out] butcher a pointer to a pointer to hold the butcher
 *
 * @return the operation error code
 */

int bt_new(bt_t ** butcher)
{
  bt_t * self;

  if (!butcher)
    return_error(EINVAL);

  self = malloc(sizeof(bt_t));
  if (!self)
    return_error(ENOMEM);

  memset(self, 0, sizeof(bt_t));

  self->elfs = NULL;

  *butcher = self;

  return 0;
}

/**
 * initializes the butcher
 *
 * @param[in] self a pointer to the butcher
 * @param[in] name the path to bexec
 * @param[in] fd the fd to direct output to
 * @param[in] smatch a regex string used to select test suites
 * @param[in] tmatch a regex string used to select tests
 *
 * @return the operation error code
 */

int bt_init(bt_t * self, const char * name, FILE * fd, const char * smatch, const char * tmatch)
{
  if (!self)
    return_error(EINVAL);

  self->fd = fd;

  self->bexec = strdup(name);
  if (!self->bexec)
    return_error(ENOMEM);


  if (smatch) {
    if (regcomp(&self->sregex, smatch, REG_EXTENDED | REG_NOSUB))
      return_error(EINVAL);
  } else { /* grab all suites by default */
    if (regcomp(&self->sregex, ".*", REG_EXTENDED | REG_NOSUB))
      return_error(EINVAL);
  }

  if (tmatch) {
    if (regcomp(&self->tregex, tmatch, REG_EXTENDED | REG_NOSUB))
      return_error(EINVAL);
  } else { /* grab all tests by default */
    if (regcomp(&self->tregex, ".*", REG_EXTENDED | REG_NOSUB))
      return_error(EINVAL);
  }

  self->initialized = 1;
  return 0;
}

/**
 * initializes the butcher
 *
 * @param[in] self a pointer to the butcher
 * @param[in] flags or-ed flags flags to set
 *
 * @return the operation error code
 */

int bt_tune(bt_t * self, unsigned int flags)
{
  if (!self || !self->initialized)
    return_error(EINVAL);

  if (flags & BT_FLAG_VERBOSE)
    self->verbose = 1;
  else
    self->verbose = 0;

  if (flags & BT_FLAG_COLOR)
    self->color = 1;
  else
    self->color = 0;

  if (flags & BT_FLAG_MESSAGES)
    self->messages = 1;
  else
    self->messages = 0;

  if (flags & BT_FLAG_ENVDUMP)
    self->envdump = 1;
  else
    self->envdump = 0;


  return 0;
}

int bt_debugger(bt_t * self, const char * dbg)
{
  if (!self || !self->initialized || !dbg)
    return_error(EINVAL);
  /*
   * self->bexec = strdup(name);
   * if (!self->bexec)
   *    return_error(ENOMEM);
   */
  const char * p, * t, * pe;
  unsigned int n = 0;

  p = dbg;

  for (pe = p; pe && *pe; pe++) ;

  for (t = p; t < pe && *t; t++) {
    if (*t == ' ')
      n++;
  }
  n++;

  self->debugger = malloc(sizeof(char *) * n);
  if (!self->debugger)
    return_error(ENOMEM);
  memset(self->debugger, 0, sizeof(char *) * n);

  for (unsigned int i = 0; i < n; i++) {
    for (t = p; t < pe && *t != ' '; t++) ;
    self->debugger[i] = malloc(t - p + 1);
    if (!self->debugger[i])
      goto failure;
    memset(self->debugger[i], 0, t - p + 1);
    memcpy(self->debugger[i], p, t - p);

    p = t + 1;
  }

  self->debugger_nargs = n;

  return 0;

failure:
  for (unsigned int i = 0; i < n; i++) {
    if (self->debugger[i])
      free(self->debugger[i]);
  }
  free(self->debugger);
  self->debugger = NULL;
  return_error(ENOMEM);

}

/**
 * loads a couple of shared objects
 *
 * @param[in] self a pointer the butcher
 * @param[in] paramc the number of filenames in the array
 * @param[in] paramv the array of filenames of shared objects
 *
 * @return the operation error code
 */

int bt_loadv(bt_t * self, int paramc, char * paramv[])
{
  int err;

  if (!self || !self->initialized || (paramc && !paramv))
    return_error(EINVAL);

  for (int i = 0; i < paramc; i++) {
    err = bt_load(self, paramv[i]);
    if (err)
      return_error(err);
  }
  return 0;
}

/**
 * loads a shared object
 *
 * @param[in] self a pointer the butcher
 * @param[in] elfname filename of the shared objects
 *
 * @return the operation error code
 */

int bt_load(bt_t * self, const char * elfname)
{
  int err;
  bt_elf_t * btelf = NULL;

  err = bt_elf_new(&btelf, self, elfname);
  if (err) /* allocation error? */
    goto failure;

  err = bt_elf_load2(btelf);
  if (err)
    goto failure;

  btelf->next = self->elfs;
  self->elfs = btelf;

  return 0;
failure:
  if (btelf)
    bt_elf_delete(&btelf);
  return_error(err);
}

/**
 * lists what the butcher was able to load
 *
 * @param[in] self a pointer the butcher
 *
 * @return the operation error code
 */

int bt_list(bt_t * self)
{
  bt_elf_t * elf_cur;
  bt_suite_t * suite_cur;
  bt_test_t * test_cur;
  unsigned n, m;

  if (!self || !self->initialized)
    return_error(EINVAL);

  fprintf(self->fd, "%slisting loaded objects%s...\n\n",
      self->color ? GREEN : "", self->color ? ENDCOL : "");

  elf_cur = self->elfs;
  while (elf_cur) {
    fprintf(self->fd, "[%self%s, name='%s%s%s']\n",
        self->color ? YELLOW : "", self->color ? ENDCOL : "",
        self->color ? RED : "", elf_cur->name, self->color ? ENDCOL : "");

    for (n = 0; n < elf_cur->hsize; n++) {
      suite_cur = elf_cur->hsuites[n];
      while (suite_cur) {
        fprintf(self->fd, " [%ssuite%s, name='%s%s%s']\n",
            self->color ? BLUE : "", self->color ? ENDCOL : "",
            self->color ? GREEN : "", suite_cur->name, self->color ? ENDCOL : "");

        for (m = 0; m < suite_cur->hsize; m++) {
          test_cur = suite_cur->htests[m];
          while (test_cur) {
            fprintf(self->fd, "  [%stest%s, name='%s%s%s'",
                self->color ? PURPLE : "", self->color ? ENDCOL : "",
                self->color ? RED : "", test_cur->name, self->color ? ENDCOL : "");
            if (test_cur->setupid != BT_NO_ID)
              fprintf(self->fd, ", setup=%d", test_cur->setupid);
            if (test_cur->teardownid != BT_NO_ID)
              fprintf(self->fd, ", setup=%d", test_cur->teardownid);
            fprintf(self->fd, ", function=%d", test_cur->id);
            fprintf(self->fd, "]\n");

            test_cur = test_cur->next;
          }
        }

        suite_cur = suite_cur->next;
      }
    }

    elf_cur = elf_cur->next;
  }

  return 0;
}

/**
 * internal function that runs a single test
 *
 * @param[in] self a pointer the butcher
 * @param[in] elf the shared object where test is defined
 * @param[in] suite the suite where test is defined
 * @param[in] test the test to run
 *
 * @return the operation error code
 */

int bt_chopper(bt_t * self, bt_elf_t * elf, bt_suite_t * suite, bt_test_t * test)
{
  int status;
  pid_t pid;
  int err;

  int pipeout[2];
  int cntlout[2];

  if (!self || !self->initialized || !test) {
    fprintf(self->fd, "no self, not initialized or no test!\n");
    return_error(EINVAL);
  }

  if (self->debugger) {
    char buf[64];
    unsigned int k = 0;
    int argc = self->debugger_nargs + 2;
    char * argv[argc];

    setenv("butcher_elf_name", elf->name, 1);
    if (test->setupid != BT_NO_ID) {
      snprintf(buf, 64, "%d", test->setupid);
      setenv("butcher_test_setup", buf, 1);
    }
    if (test->setupid != BT_NO_ID) {
      snprintf(buf, 64, "%d", test->teardownid);
      setenv("butcher_test_teardown", buf, 1);
    }
    if (test->id != BT_NO_ID) {
      snprintf(buf, 64, "%d", test->id);
      setenv("butcher_test_function", buf, 1);
    }
    setenv("butcher_verbose", self->messages ? "true" : "false", 1);

    k = 0;
    for (; k < self->debugger_nargs; k++)
      argv[k] = self->debugger[k];

    argv[k++] = self->bexec;
    argv[k] = NULL;

    fprintf(self->fd, "running suite '%s', test '%s' \n", suite->name, test->name);

    execvp(argv[0], argv);

    /* not reached */
    fprintf(self->fd, "could not call execv/execvp() with:\n");
    for (k = 0; argv[k]; k++) {
      fprintf(self->fd, "  ARG %d: %s\n", k, argv[k]);
    }

    exit(-1);
  }

  err = bt_log_new(&test->log);
  if (err) {
    fprintf(self->fd, "could not create ne log\n");
    return_error(err);
  }

  if (pipe2(pipeout, O_NONBLOCK)) {
    fprintf(self->fd, "could not create log pipe\n");
    return_error(errno);
  }
  if (pipe2(cntlout, O_NONBLOCK)) {
    fprintf(self->fd, "could not create control pipe\n");
    return_error(errno);
  }


  fprintf(self->fd, "running suite '%s', test '%s'...\r", suite->name, test->name);

  pid = fork();

  if (pid == -1) {
    return_error(ENAVAIL);
  } else if (pid == 0) {
    /* forked here */
    char * chunk;
    size_t chunklen, pos;
    char * env[16] = {NULL};
    unsigned int e;

    chunklen = 0;

    if (self->bexec)
      chunklen += strlen(self->bexec) + 2;
    if (elf->name)
      chunklen += strlen("butcher_elf_name") + strlen(elf->name) + 2;

    if (test->setupid != BT_NO_ID)
      chunklen += strlen("butcher_test_setup") + 10 + 2;

    if (test->teardownid != BT_NO_ID)
      chunklen += strlen("butcher_test_teardown") + 10 + 2;

    if (test->id != BT_NO_ID)
      chunklen += strlen("butcher_test_function") + 10 + 2;

    chunklen += strlen("butcher_verbose") + strlen("false") + 2;
    chunklen += strlen("butcher_envdump") + strlen("false") + 2;

    if (getenv("LD_LIBRARY_PATH"))
      chunklen += strlen("LD_LIBRARY_PATH") + strlen(getenv("LD_LIBRARY_PATH")) + 2;

    chunk = malloc(chunklen);
    if (!chunk)
      exit(-1);

    pos = 0; e = 0;
    if (elf->name) {
      snprintf(chunk + pos, chunklen - pos, "butcher_elf_name=%s", elf->name);
      env[e++] = chunk + pos; pos += strlen(chunk + pos) + 1;
    }

    if (test->setupid != BT_NO_ID) {
      snprintf(chunk + pos, chunklen - pos, "butcher_test_setup=%d", test->setupid);
      env[e++] = chunk + pos; pos += strlen(chunk + pos) + 1;
    }

    if (test->teardownid != BT_NO_ID) {
      snprintf(chunk + pos, chunklen - pos, "butcher_test_teardown=%d", test->teardownid);
      env[e++] = chunk + pos; pos += strlen(chunk + pos) + 1;
    }

    if (test->id != BT_NO_ID) {
      snprintf(chunk + pos, chunklen - pos, "butcher_test_function=%d", test->id);
      env[e++] = chunk + pos; pos += strlen(chunk + pos) + 1;
    }

    snprintf(chunk + pos, chunklen - pos, "butcher_cfd=%d", cntlout[1]);
    env[e++] = chunk + pos; pos += strlen(chunk + pos) + 1;

    snprintf(chunk + pos, chunklen - pos, "butcher_verbose=%s", self->messages ? "true" : "false");
    env[e++] = chunk + pos; pos += strlen(chunk + pos) + 1;

    snprintf(chunk + pos, chunklen - pos, "butcher_envdump=%s", self->envdump ? "true" : "false");
    env[e++] = chunk + pos; pos += strlen(chunk + pos) + 1;

    if (getenv("LD_LIBRARY_PATH")) {
      snprintf(chunk + pos, chunklen - pos, "LD_LIBRARY_PATH=%s", getenv("LD_LIBRARY_PATH"));
      env[e++] = chunk + pos; pos += strlen(chunk + pos) + 1;
    }

    char * argv[2] = {self->bexec, NULL};

    /* redirect stdout */
    close(pipeout[0]);
    dup2(pipeout[1], STDOUT_FILENO);
    dup2(pipeout[1], STDERR_FILENO);
    close(pipeout[1]);
    close(STDOUT_FILENO);
    close(cntlout[0]); /* close read end of control stream */
    close(STDIN_FILENO);

    execve(argv[0], argv, env);

    /* not reached */
    fprintf(self->fd, "could not call execve() with:\n");
    unsigned int k;
    for (k = 0; env[k]; k++) {
      fprintf(self->fd, "  ENV %d: %s\n", k, env[k]);
    }
    for (k = 0; argv[k]; k++) {
      fprintf(self->fd, "  ARG %d: %s\n", k, argv[k]);
    }
    exit(-1);
  } else {
    struct result_rec rec = {
      "\x01\x02\x03\x04",
      {BT_TEST_NONE, BT_TEST_NONE, BT_TEST_NONE},
      0
    };
    char            * buffer, * tmp;
    char              c;
    size_t            buffer_length;
    size_t            buffer_length_new;
    size_t            buffer_cur;
    size_t            i, n;
    size_t            bytes;
    ssize_t           length;
    pid_t             waitret;
    int               running;

    close(pipeout[1]); /* close write end of log stream */
    close(cntlout[1]); /* close write end of control stream */

    buffer = NULL;
    buffer_length = 512;
    buffer_length_new = 512;
    buffer_cur = 0;
    bytes = 0;
    running = 1;


    do {
      usleep(100);

      waitret = wait4(pid, &status, WNOHANG, &test->ru);
      if (waitret == -1)
        goto loop_wait_fail;

      if (waitret == pid)
        running = 0;

      err = ioctl(cntlout[0], FIONREAD, &bytes);
      if (err)
        goto loop_io_failure;
      for (size_t n = 0; n < bytes; n+= sizeof(rec)) {
        read(cntlout[0], &rec, sizeof(rec));
      }

      err = ioctl(pipeout[0], FIONREAD, &bytes);
      if (err)
        goto loop_io_failure;

      if (buffer_length - buffer_cur < bytes) {
        buffer_length_new = buffer_length * 2;
        while (buffer_length_new - buffer_cur < bytes) {
          buffer_length_new = buffer_length_new * 2;
        }
      }

      if (!buffer || buffer_length != buffer_length_new) {
        tmp = realloc(buffer, buffer_length_new + 1);
        if (!tmp) {
          err = ENOMEM;
          goto loop_oom_failure;
        }
        buffer = tmp; tmp = NULL;
        buffer_length = buffer_length_new;
      }

      length = read(pipeout[0], buffer + buffer_cur, bytes);
      if (self->debugger)
        fprintf(stderr, "%.*s", (int) bytes, buffer + buffer_cur);
      buffer_cur += length;

    } while (running);

    close(pipeout[0]); /* close read end of log stream */
    close(pipeout[0]); /* close read end of control stream */

    if (buffer) {
      /* terminate the buffer */
      buffer[buffer_cur] = '\0';
    }

    i = 0;
    while (i < buffer_cur) {
      for (n = i; n < buffer_cur && (c = buffer[n]) != '\n' && c != '\r' && c != '\0'; n++) ;
      err = bt_log_msgcpy(test->log, buffer + i, n - i);
      if (err)
        goto parse_failure;
      i = n + 1;
    }

    free(buffer);

    if (WIFEXITED(status)) {
      if (!rec.done) {
        for (int i = 0; i < BT_PASS_MAX; i++) {
          test->results[i] = BT_TEST_CORRUPTED;
        }
        char msg[32];
        snprintf(msg, 32, "(test was aborted)");
        bt_log_msgcpy(test->log, msg, -1);
        fprintf(self->fd, "running suite '%s', test '%s'... aborted (how could that happen?!)\n",
          suite->name, test->name);
        return 0;
      }

      fprintf(self->fd, "running suite '%s', test '%s'... ", suite->name, test->name);
      int max = BT_TEST_NONE;
      for (int i = 0; i < BT_PASS_MAX; i++) {
        test->results[i] = rec.results[i];
        if (max < test->results[i]) {
          max = test->results[i];
        }
      }
      if (max == BT_TEST_SUCCEEDED) {
        fprintf(self->fd, "passed\n");
      } else {
        fprintf(self->fd, "failed\n");
      }
    } else if (WIFSIGNALED(status)) {
      for (int i = 0; i < BT_PASS_MAX; i++) {
        if (rec.results[i] > BT_TEST_NONE)
          test->results[i] = rec.results[i];
        else {
          test->results[i] = BT_TEST_CORRUPTED;
          break;
        }
      }
      char msg[32];
      snprintf(msg, 32, "(exited with signal %d)", WTERMSIG(status));
      bt_log_msgcpy(test->log, msg, -1);
      fprintf(self->fd, "running suite '%s', test '%s'... signaled!\n", suite->name, test->name);
    }

    return 0;

loop_wait_fail:
    if (!self->debugger)
      close(pipeout[0]);
    err = EINVAL;
    goto failure;

loop_io_failure:
    if (!self->debugger)
      close(pipeout[0]);
    err = errno;
    goto failure;

loop_oom_failure:
    if (!self->debugger)
      close(pipeout[0]);
    err = ENOMEM;
    goto failure;

parse_failure:
    goto failure;

    /* failed */
failure:
    if (buffer)
      free(buffer);
    return_error(err);
  }
}

/**
 * performs loaded tests
 *
 * @param[in] self a pointer the butcher
 *
 * @return the operation error code
 */

int bt_chop(bt_t * self)
{
  bt_elf_t * elf_cur;
  bt_suite_t * suite_cur;
  bt_test_t * test_cur;
  int err;
  unsigned n, m;



  if (!self || !self->initialized)
    return_error(EINVAL);

  elf_cur = self->elfs;
  while (elf_cur) {
    for (n = 0; n < elf_cur->hsize; n++) {
      suite_cur = elf_cur->hsuites[n];
      while (suite_cur) {
        if (!regexec(&self->sregex, suite_cur->name, 0, NULL, 0)) {
          for (m = 0; m < suite_cur->hsize; m++) {
            test_cur = suite_cur->htests[m];
            while (test_cur) {
              if (!regexec(&self->tregex, test_cur->name, 0, NULL, 0)) {
                err = bt_chopper(self, elf_cur, suite_cur, test_cur);
                if (err)
                  return_error(err);
              }

              test_cur = test_cur->next;
            }
          }
        }

        suite_cur = suite_cur->next;
      }
    }

    elf_cur = elf_cur->next;
  }
  return 0;
}

/**
 * reports results of the chopper phase
 *
 * @param[in] self a pointer the butcher
 *
 * @return the operation error code
 */

int bt_report(bt_t * self)
{
  bt_elf_t * elf_cur;
  bt_suite_t * suite_cur;
  bt_test_t * test_cur;
  bt_log_line_t * line_cur;

  if (!self || !self->initialized)
    return_error(EINVAL);

  unsigned int    allresults[BT_TEST_MAX] = {0};
  int             allcount = 0;
  unsigned n, m;

  if (self->verbose)
    fprintf(self->fd, "%slisting results for loaded objects%s (worst counts)...\n\n",
        self->color ? GREEN : "", self->color ? ENDCOL : "");

  elf_cur = self->elfs;
  while (elf_cur) {
    fprintf(self->fd, "[%self%s, name='%s%s%s']\n",
        self->color ? YELLOW : "", self->color ? ENDCOL : "",
        self->color ? RED : "", elf_cur->name, self->color ? ENDCOL : "");

    for (n = 0; n < elf_cur->hsize; n++) {
      suite_cur = elf_cur->hsuites[n];
      while (suite_cur) {
        fprintf(self->fd, " [%ssuite%s, name='%s%s%s']\n",
            self->color ? BLUE : "", self->color ? ENDCOL : "",
            self->color ? GREEN : "", suite_cur->name, self->color ? ENDCOL : "");

        unsigned int results[BT_TEST_MAX] = {0};
        int          result;
        int          count = 0;

        for (m = 0; m < suite_cur->hsize; m++) {
          test_cur = suite_cur->htests[m];
          while (test_cur) {
            result = BT_TEST_NONE;
            for (int i = 0; i < BT_PASS_MAX; i++) {
              if (test_cur->results[i] > result)
                result = test_cur->results[i];
            }

            if (result > BT_TEST_SUCCEEDED) {
              fprintf(self->fd, "  [%stest%s, name='%s%s%s']\n",
                  self->color ? PURPLE : "", self->color ? ENDCOL : "",
                  self->color ? RED : "", test_cur->name, self->color ? ENDCOL : "");
            }

            if ((self->messages || result > BT_TEST_SUCCEEDED) && test_cur->log) {
              line_cur = test_cur->log->lines;
              while (line_cur) {
                if (result > BT_TEST_SUCCEEDED) {
                  fprintf(self->fd, "   %s%s%s\n",
                      self->color ? RED : "", line_cur->contents, self->color ? ENDCOL : "");
                } else {
                  fprintf(self->fd, "   %s\n", line_cur->contents);
                }
                line_cur = line_cur->next;
              }
            }

            if ((self->verbose && result > BT_TEST_NONE) || result > BT_TEST_SUCCEEDED) {
              fprintf(self->fd, "   U+S:%lu U:%lu S:%lu MRSS:%ld IXRSS:%ld DU:%ld SU:%ld SPF:%ld PF:%ld SW:%ld OI:%ld OO:%ld MS:%ld MR:%ld SD:%ld\n",
                (unsigned long)(test_cur->ru.ru_utime.tv_sec * 1000000) + test_cur->ru.ru_utime.tv_usec
                  + (unsigned long)(test_cur->ru.ru_stime.tv_sec * 1000000) + test_cur->ru.ru_stime.tv_usec,
                (unsigned long)(test_cur->ru.ru_utime.tv_sec * 1000000) + test_cur->ru.ru_utime.tv_usec,
                (unsigned long)(test_cur->ru.ru_stime.tv_sec * 1000000) + test_cur->ru.ru_stime.tv_usec,
                test_cur->ru.ru_maxrss,
                test_cur->ru.ru_ixrss,
                test_cur->ru.ru_idrss,
                test_cur->ru.ru_isrss,
                test_cur->ru.ru_minflt,
                test_cur->ru.ru_majflt,
                test_cur->ru.ru_nswap,
                test_cur->ru.ru_inblock,
                test_cur->ru.ru_oublock,
                test_cur->ru.ru_msgsnd,
                test_cur->ru.ru_msgrcv,
                test_cur->ru.ru_nsignals
              );

              fprintf(self->fd, "   -> results: ");
            }

            if (self->messages || result > BT_TEST_SUCCEEDED) {
              for (int i = 0, flag = 0; i < BT_PASS_MAX; i++) {
                if (test_cur->results[i] > BT_TEST_NONE) {
                  if (flag)
                    fprintf(self->fd, "               ");
                  else
                    flag = 1;

                  switch (i) {
                    case BT_PASS_SETUP:
                      fprintf(self->fd,
                        "%ssetup%s ",
                        self->color ? CYAN : "",
                        self->color ? ENDCOL : ""); break;
                    case BT_PASS_TEST:
                      fprintf(self->fd, "%stest%s ", self->color ? CYAN : "", self->color ? ENDCOL : "");
                      break;
                    case BT_PASS_TEARDOWN:
                      fprintf(self->fd,
                        "%steardown%s ",
                        self->color ? CYAN : "",
                        self->color ? ENDCOL : ""); break;
                    default:
                      break;
                  }

                  switch (test_cur->results[i]) {
                    case BT_TEST_SUCCEEDED:
                      fprintf(self->fd,
                        "%ssucceeded%s\n",
                        self->color ? GREEN : "",
                        self->color ? ENDCOL : ""); break;
                    case BT_TEST_IGNORED:
                      fprintf(self->fd,
                        "%signored%s\n",
                        self->color ? YELLOW : "",
                        self->color ? ENDCOL : ""); break;
                    case BT_TEST_FAILED:
                      fprintf(self->fd, "%sfailed%s\n",
                        self->color ? RED : "",
                        self->color ? ENDCOL : ""); break;
                    case BT_TEST_CORRUPTED:
                      fprintf(self->fd,
                        "%scorrupted%s\n",
                        self->color ? RED : "",
                        self->color ? ENDCOL : ""); break;
                    default:
                      break;
                  }
                }
              }
            }

            if ((self->verbose && result > BT_TEST_NONE) || result > BT_TEST_SUCCEEDED) {
              if (self->messages || result > BT_TEST_SUCCEEDED)
                fprintf(self->fd, "               ");
              switch (result) {
                case BT_TEST_SUCCEEDED:
                  fprintf(self->fd,
                    " -> [%ssucceeded%s]",
                    self->color ? GREEN CYAN_BG : "",
                    self->color ? ENDCOL : ""); break;
                case BT_TEST_IGNORED:
                  fprintf(self->fd,
                    " -> [%signored%s]",
                    self->color ? GREEN CYAN_BG : "",
                    self->color ? ENDCOL : ""); break;
                case BT_TEST_FAILED:
                  fprintf(self->fd,
                    " -> [%sfailed%s]",
                    self->color ? RED_BG : "",
                    self->color ? ENDCOL : ""); break;
                case BT_TEST_CORRUPTED:
                  fprintf(self->fd,
                    " -> [%scorrupted%s]",
                    self->color ? RED_BG : "",
                    self->color ? ENDCOL : ""); break;
                default:
                  break;
              }
            }

            if ((self->verbose && result > BT_TEST_NONE) || result > BT_TEST_SUCCEEDED)
              fprintf(self->fd, "\n");

            if (result > BT_TEST_NONE) {
              results[result]++;
              count++;
            }

            test_cur = test_cur->next;
          }
        }

        for (int i = 0; i < BT_TEST_MAX; i++) {
          allresults[i] += results[i];
        }
        allcount += count;

        if (count) {
          int choice = results[BT_TEST_IGNORED] + results[BT_TEST_FAILED] == 0;
          fprintf(
              self->fd,
              "  => %s%d%s/%d test%s succeeded (%g%%) [%d ignored, %d failed, %d corrupted]\n",
              self->color ? (choice ? GREEN : RED) : "", results[BT_TEST_SUCCEEDED], self->color ? ENDCOL : "",
              count, count <= 1 ? "" : "s",
              (double) results[BT_TEST_SUCCEEDED] / count * 100,
              results[BT_TEST_IGNORED],
              results[BT_TEST_FAILED],
              results[BT_TEST_CORRUPTED]);
        }
        if (self->messages)
          fprintf(self->fd, "  \n");

        suite_cur = suite_cur->next;
      }
    }

    elf_cur = elf_cur->next;
  }

  {
    int choice = allresults[BT_TEST_IGNORED] + allresults[BT_TEST_FAILED] == 0;
    fprintf(
        self->fd,
        " => %s%d%s/%d test%s succeeded (%g%%) [%d ignored, %d failed, %d corrupted]\n",
        self->color ? (choice ? GREEN : RED) : "", allresults[BT_TEST_SUCCEEDED], self->color ? ENDCOL : "",
        allcount, allcount <= 1 ? "" : "s",
        (double) allresults[BT_TEST_SUCCEEDED] / allcount * 100,
        allresults[BT_TEST_IGNORED],
        allresults[BT_TEST_FAILED],
        allresults[BT_TEST_CORRUPTED]);
  }

  return 0;
}

/**
 * recursively deletes the butcher
 *
 * @param[in] butcher a pointer to a pointer holding the butcher
 *
 * @return the operation error code
 */

int bt_delete(bt_t ** butcher)
{
  bt_t * self;

  if (!butcher || !*butcher)
    return_error(EINVAL);

  self = *butcher;

  regfree(&self->sregex);
  regfree(&self->tregex);

  bt_elf_t * cur, * tmp;
  cur = self->elfs;
  while (cur) {
    tmp = cur->next;
    bt_elf_delete(&cur);
    cur = tmp;
  }

  free(self->bexec);
  for (unsigned int i = 0; i < self->debugger_nargs; i++) {
    if (self->debugger[i])
      free(self->debugger[i]);
  }
  free(self->debugger);

  free(self);

  return 0;
}
