/* @@INCLUDE-HEADER@@ */

#ifndef BTPRIVATE_H_
#define BTPRIVATE_H_

#include "bt.h"
#include <sys/resource.h>
#include <sys/wait.h>
#include <regex.h>

enum {
  BT_PASS_SETUP = 0, /* enable array access */
  BT_PASS_TEST,
  BT_PASS_TEARDOWN,
  BT_PASS_MAX, /* define array size */
};

enum {
  BT_TEST_NONE = -1,
  BT_TEST_SUCCEEDED = 0,
  BT_TEST_FAILED,
  BT_TEST_IGNORED,
  BT_TEST_CORRUPTED,
  BT_TEST_MAX
};

typedef struct bt_log_line bt_log_line_t;
typedef struct bt_log bt_log_t;
typedef struct bt_test bt_test_t;
typedef struct bt_suite bt_suite_t;
typedef struct bt_elf bt_elf_t;

/*
 * variable sized C9x structure holding a read-only message
 */
struct bt_log_line {
  struct bt_log_line * next;
  char contents[];
};

/*
 * structure holding a linked list of log messages and an pointer
 * to the last for easy appending
 */
struct bt_log {
  struct bt_log_line * lines;
  struct bt_log_line * last;
};

/*
 * structure holding a test case, which consists of:
 *  - a test function
 *  - a log
 *  - an array of results
 *  - a name of the test case
 */
struct bt_test {
  struct bt_test * next;
  unsigned         id;
  bt_fn_kind_t     kind;
  char           * name;
  char             results[BT_PASS_MAX];
  bt_log_t       * log;
  struct rusage    ru;

  unsigned setupid;
  unsigned teardownid;
};

/*
 * structure holding a test suite containing:
 *  - an optional setup function
 *  - an optional teardown function
 *  - a list of test cases
 */
struct bt_suite {
  struct bt_suite * next;

  bt_test_t ** htests;
  unsigned     hsize;

  char * name;
};

/*
 * structure holding a dl-opened shared object (which contains test suites)
 * containing a handle to received from dlopen() and a list of test suites
 * derived from test declarations
 */
struct bt_elf {
  struct bt_elf * next;
  bt_t        * butcher;
  char        * name;
  bt_suite_t ** hsuites;
  unsigned      hsize;
  void        * dlhandle;
};

/*
 * the butcher holding [a big knife and]
 *  - a list of shared objects (to chop)
 */
struct bt {
  bt_elf_t * elfs;
  char color;
  char verbose;
  char messages;
  char envdump;
  char initialized;

  FILE * fd;

  char * bexec;
  char ** debugger;
  unsigned int debugger_nargs;

  regex_t sregex, tregex;
};

#define BT_NO_ID ((unsigned) -1)

struct result_rec {
  char magic[5];
  char results[BT_PASS_MAX];
  char done;
};
#endif /* BTPRIVATE_H_ */
