/* @@INCLUDE-HEADER@@ */

#ifndef BT_H_
#define BT_H_

/*
 * The BUTCHER public interface...
 */

#define BAPI extern
#include <errno.h>
#include <stdio.h>

/*#define return_error(_error) return _error*/
#define return_error(_error) \
  do { \
    fprintf(stderr, "ERROR %d in %s\n", _error, __FUNCTION__); \
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

typedef int (bt_test_function_t)(void *, void **);

struct bt_tester {
  /* pipe descriptor the log function should write to */
  int fd;
  /* pipe descriptor for the control stream */
  int cfd;

  /* these should be considered private */
  bt_test_function_t * setup;
  bt_test_function_t * function;
  bt_test_function_t * teardown;
};

typedef struct bt bt_t;

typedef struct bt_fn bt_fn_t;

struct bt_fn {
  const char * name;
  const char * extra;
  unsigned long flags;
  int (* function)(void *, void **);
};

typedef enum {
  BT_FN_KIND_PTEST,
  BT_FN_KIND_FTEST,
  BT_FN_KIND_SETUP,
  BT_FN_KIND_TEARDOWN,
} bt_fn_kind_t;


BAPI int bt_new(bt_t ** butcher);

BAPI int bt_init(bt_t * butcher,
    const char * name,
    FILE * fd,
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

BAPI void bt_backtrace();

/**
 *
 * the layout embedded inside a shared object created from:
 * ~~~snip~~~
 * BT_EXPORT();
 * BT_TEST(<suite>, <test 1>) {...}
 * BT_TEST_FIXTURE(<suite>, <test 2>, <setup>, <teardown>)
 * ...
 * ~~~snap~~~
 * boils down to:
 * ~~~snip~~~
 * elf.so
 *  bt_fn bexec[] = {
 *    {"<test 1>", "<suite>", BT_FN_KIND_PTEST, <test 1>},
 *    {"<test 2>", "<suite>", BT_FN_KIND_SETUP, <setup>},
 *    {"<test 2>", "<suite>", BT_FN_KIND_TEARDOWN, <teardown>},
 *    {"<test 2>", "<suite>", BT_FN_KIND_FTEST, <test 2>},
 *    ...
 *  }
 * ~~~snap~~~
 */

/* client interface */

/* macro voodoo */

#define BT_TEST(_suite, _name) \
  static int _name(); \
  static const bt_fn_t _name##rec __attribute__ ((section ("bexec"))) = { \
    #_name, \
    #_suite, \
    BT_FN_KIND_PTEST, \
    _name, \
  };\
  static int _name()

#define BT_TEST_FIXTURE(_suite, _name, _setup, _teardown, _arg) \
  static const bt_fn_t _name##_setup_rec __attribute__ ((section ("bexec"))) = { \
    #_name, \
    #_suite, \
    BT_FN_KIND_SETUP, \
    _setup, \
  };\
  static const bt_fn_t _name##_teardown_rec __attribute__ ((section ("bexec"))) = { \
    #_name, \
    #_suite, \
    BT_FN_KIND_TEARDOWN, \
    _teardown, \
  };\
  static int _name(void *); \
  static const bt_fn_t _name##rec __attribute__ ((section ("bexec"))) = { \
    #_name, \
    #_suite, \
    BT_FN_KIND_FTEST, \
    (int (*)(void *, void **)) _name, \
  };\
  static int _name(void * _arg)

#define BT_EXPORT() \
  extern const struct test __start_bexec, __stop_bexec;\
  __attribute__((used)) \
  static const struct test * __export_bexec[2] = {&__start_bexec, &__stop_bexec}


/* test functors */

#include <stdio.h>
#include <unistd.h>

#define bt_log(...) \
  do { \
    fprintf(stdout, __VA_ARGS__); \
  } while (0)

#define bt_assert(__expr) \
  do { \
    if (!(__expr)) { \
      bt_backtrace(); \
      printf( \
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
      bt_backtrace(); \
      printf("%s:%s:%d:\n  Assertion failed: expeced " # __actual \
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
      bt_backtrace(); \
      printf("%s:%s:%d:\n  Assertion failed: expeced " # __actual \
          " "__not "to be " __fmt ", got " __fmt __extra "\n", \
          __FILE__, __FUNCTION__, __LINE__, \
          (__type) exp, (__type) act); \
      return BT_RESULT_FAIL; \
    } \
  } while (0)

#define bt_assert_str_equal(__actual, __expected) \
  do { \
    if (strcmp((__actual), (__expected)) != 0) { \
      bt_backtrace(); \
      printf("%s:%s:%d:\n  Assertion failed: expeced '%s' , got '%s' \n", \
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
