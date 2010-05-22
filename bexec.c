
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include <dlfcn.h>

#include "bt-private.h"

bt_tester_t tester;

int main(int argc, char *argv[]) 
{
	void * dl_handle;
	void * object;
	int result, test_result;

	if (argc != 5)
		exit(-1);

	char * dl_lib = argv[1];
	char * dl_setup = argv[2];
	char * dl_teardown = argv[3];
	char * dl_test = argv[4];

	if ((dl_handle = dlopen(dl_lib, RTLD_NOW)) == NULL) {
		dprintf(2, "ERROR: dlopen returned %s\n", dlerror());
		exit(-1);
	}

	void * ptr;

	if ((ptr = dlsym(dl_handle, dl_setup)) == NULL)
		dlerror();
	*(void **)&tester.setup = ptr;
	if ((ptr = dlsym(dl_handle, dl_teardown)) == NULL)
		dlerror();
	*(void **)&tester.teardown = ptr;	
	if ((ptr = dlsym(dl_handle, dl_test)) == NULL) {
		dlerror();
		exit(-1);
	}
	*(void **)&tester.function = ptr;

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

	/* does not make much sense to run the test if setup failed */
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

	dlclose(dl_handle);
}
