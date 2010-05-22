
#include <bt.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

BT_SUITE_DEF(foo_test,"sample butcher test suite");

BT_SUITE_SETUP_DEF(foo_test)
{
	bt_log("entered setup\n");

	char * buffer = malloc(1024);

	*object = buffer;
	
	bt_log("leaving setup\n");
	return BT_RESULT_OK;
}

BT_TEST_DEF(foo_test, empty, "an empty test")
{
	return BT_RESULT_OK;
}

BT_TEST_DEF(foo_test, sigsegv, "corrupted test")
{
	int *i=NULL;
	*i = 1;
	return BT_RESULT_OK;
}

BT_TEST_DEF(foo_test, longtest, "long test")
{
	for (int i = 0; i < 100; i++)
		usleep(100);

	return BT_RESULT_OK;
}


BT_SUITE_TEARDOWN_DEF(foo_test)
{
	bt_log("entered teardown\n");

	char * buffer = *object;

	free(buffer);

	*object = NULL;

	bt_log("leaving teardown\n");
	return BT_RESULT_OK;
}


