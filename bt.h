/* @@INCLUDE-HEADER@@ */

#ifndef BT_H_
#define BT_H_

/*
 * The BUTCHER public interface...
 */

#define BAPI extern
#include <errno.h>

/*#define return_error(_error) return _error*/
#define return_error(_error) \
  do { \
    dprintf(2, "ERROR %d in %s\n", _error, __FUNCTION__); \
    return _error; \
  } while (0)

#define UNUSED_PARAM(x) (void) (x)

enum {
  BT_RESULT_OK = 0,
  BT_RESULT_IGNORE,
  BT_RESULT_FAIL
};


#define BT_FLAG_VERBOSE (1 << 0)
#define BT_FLAG_COLOR (1 << 1)
#define BT_FLAG_DESCRIPTIONS (1 << 2)
#define BT_FLAG_MESSAGES (1 << 3)
#define BT_FLAG_ENVDUMP (1 << 4)

typedef struct bt_tester bt_tester_t;

#define BT_SETUP_FUNCTION_ARG void **
#define BT_TEST_FUNCTION_ARG void *

typedef int (bt_setup_function_t)(BT_SETUP_FUNCTION_ARG);
typedef int (bt_test_function_t)(BT_TEST_FUNCTION_ARG);

struct bt_tester {
  /*
   *  pipe descriptors the log function should write to
   *  (as always: read end, write and)
   */
  int fd;

  /* these should be considered private */
  bt_setup_function_t * setup;
  bt_test_function_t * function;
  bt_setup_function_t * teardown;
};

typedef struct bt bt_t;

BAPI int bt_new(bt_t ** butcher);

BAPI int bt_init(bt_t * butcher,
    const char * name,
    int fd,
    const char * smatch,
    const char * tmatch);
BAPI int bt_tune(bt_t * butcher, unsigned int flags);
BAPI int bt_debugger(bt_t * butcher, const char * path);

BAPI int bt_loadv(bt_t * self, int paramc, char * paramv[]);
BAPI int bt_load(bt_t * butcher, const char * elfname);

BAPI int bt_list(bt_t * butcher);
BAPI int bt_chop(bt_t * butcher);
BAPI int bt_report(bt_t * butcher);

BAPI int bt_delete(bt_t ** butcher);

/*
 *
 * the layout embedded inside a shared object created from:
 * ~~~snip~~~
 * BT_SUITE_DEF(<name>, "suite description");
 * BT_SUITE_SETUP_DEF(<name>) {...}
 * BT_SUITE_TEARDOWN_DEF(<name>) {...}
 * BT_TEST_DEF(<name>, <test-name_1>, "test 1 description") {...}
 * ...
 * ~~~snap~~~
 * boils down to:
 * ~~~snip~~~
 * elf.so
 * __bt_suite_def_<name> => "suite description" { const char [] }
 *  __bt_suite_<name>_setup()
 *  __bt_suite_<name>_teardown()
 *
 *    __bt_suite_<name>_test_<test-name_1>()
 *      __bt_suite_<name>_test_<test-name_1>_descr => "test 1 description" { const char [] }
 *    ...
 * ~~~snap~~~
 */

/* client interface */

/* macro voodoo */
#define BT_SUITE_DEF(__sname, __sdesc) \
  const char __bt_suite_def_ ## __sname[] = __sdesc

#define BT_SUITE_SETUP_DEF(__sname, __objrefname) \
  int __bt_suite_ ## __sname ## _setup(BT_SETUP_FUNCTION_ARG __objrefname)

#define BT_SUITE_TEARDOWN_DEF(__sname, __objrefname) \
  int __bt_suite_ ## __sname ## _teardown(BT_SETUP_FUNCTION_ARG __objrefname)

#define BT_TEST_DEF(__sname, __tname, __objname, __tdesc) \
  const char __bt_suite_ ## __sname ## _test_ ## __tname ## _descr[] = __tdesc; \
  int __bt_suite_ ## __sname ## _test_ ## __tname(BT_TEST_FUNCTION_ARG __objname)

#define BT_TEST_DEF_PLAIN(__sname, __tname, __tdesc) \
  const char __bt_suite_ ## __sname ## _test_ ## __tname ## _descr[] = __tdesc; \
  int __bt_suite_ ## __sname ## _test_ ## __tname()

#define BT_TEST(__sname, __tname) \
  __bt_suite_ ## __sname ## _test_ ## __tname(object)


/* test functors */

#include <stdio.h>
#include <unistd.h>

#define bt_log(...) \
  do { \
    dprintf(STDOUT_FILENO, __VA_ARGS__); \
  } while (0)

#define bt_assert(__expr) \
  do { \
    if (!(__expr)) { \
      dprintf(STDOUT_FILENO, \
          "%s:%s:%d: Assertion " # __expr " failed\n", \
          __FILE__, \
          __FUNCTION__, \
          __LINE__); \
      return BT_RESULT_FAIL; \
    } \
  } while (0)


#define _bt_assert_type_equal(__type, __fmt, __actual, __expected, __not, __extra) \
  do { \
    if (!((__actual) == (__expected))) { \
      dprintf(STDOUT_FILENO, "%s:%s:%d:\n  Assertion failed: expeced " # __actual \
          " to be " __not __fmt ", got " __fmt __extra "\n", \
          __FILE__, __FUNCTION__, __LINE__, \
          (__type) __expected, (__type) __actual); \
      return BT_RESULT_FAIL; \
    } \
  } while (0)

#define _bt_assert_type_not_equal(__type, __fmt, __actual, __expected, __not, __extra) \
  do { \
    __type act = (__actual); \
    __type exp = (__expected);\
    if (((act) == (exp))) { \
      dprintf(STDOUT_FILENO, "%s:%s:%d:\n  Assertion failed: expeced " # __actual \
          " "__not "to be " __fmt ", got " __fmt __extra "\n", \
          __FILE__, __FUNCTION__, __LINE__, \
          (__type) exp, (__type) act); \
      return BT_RESULT_FAIL; \
    } \
  } while (0)

#define bt_assert_str_equal(__actual, __expected) \
  do { \
    if (strcmp((__actual), (__expected)) != 0) { \
      dprintf(STDOUT_FILENO, "%s:%s:%d:\n  Assertion failed: expeced '%s' , got '%s' \n", \
          __FILE__, __FUNCTION__, __LINE__, \
          (__expected), (__actual)); \
      return BT_RESULT_FAIL; \
    } \
  } while (0)


#define bt_assert_int_equal(__actual, __expected) \
  _bt_assert_type_equal(signed long int, "%ld", __actual, __expected, "", "")

#define bt_assert_int_not_equal(__actual, __expected) \
  _bt_assert_type_not_equal(signed long int, "%ld", __actual, __expected, "not ", "")

#define bt_nmd_assert_int_equal(__actual, __expected, __name) \
  _bt_assert_type_equal(signed long int, \
    "%ld", \
    __actual, \
    __expected, \
    "", \
    " (test label: " __name ")") \

#define bt_nmd_assert_int_not_equal(__actual, __expected, __name) \
  _bt_assert_type_not_equal(signed long int, \
    "%ld", \
    __actual, \
    __expected, \
    "not ", \
    " (test label: " __name ")") \

#define bt_assert_err_equal bt_assert_int_equal
#define bt_assert_err_not_equal bt_assert_int_not_equal

#define bt_assert_ptr_equal(__actual, __expected) \
  _bt_assert_type_equal(void *, "%p", __actual, __expected, "", "")

#define bt_assert_ptr_not_equal(__actual, __expected) \
  _bt_assert_type_not_equal(void *, "%p", __actual, __expected, "not ", "")

#define bt_assert_bool_equal(__actual, __expected) \
  _bt_assert_type_equal(int, "%d", __actual, __expected, "", "")

#endif /* BT_H_ */
