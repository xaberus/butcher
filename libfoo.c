
#include <bt.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

BT_EXPORT();

static int test_foo_setup(void * o, void ** op)
{
  printf("setup!\n");
  o = malloc(512);
  *op = o;
  return BT_RESULT_OK;
}

static int test_foo_teardown(void * o, void ** op)
{
  printf("teardown!\n");
  free(o);
  *op = NULL;
  return BT_RESULT_OK;
}

BT_TEST_FIXTURE(foosuite, test_foo, test_foo_setup, test_foo_teardown, o)
{
  bt_assert(o);
  return BT_RESULT_OK;
}

BT_TEST(foosuite, test_bar)
{
  return BT_RESULT_OK;
}

BT_TEST(foosuite, test_baz)
{
  return BT_RESULT_OK;
}

BT_TEST(bananas, take_banana)
{
  return BT_RESULT_OK;
}