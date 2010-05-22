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
#include <sys/wait.h>
#include <sys/ioctl.h>

#include <libelf.h>
#include <gelf.h>

static int bt_log_line_new(bt_log_line_t ** line, const char * msg, ssize_t len);
static int bt_log_line_delete(bt_log_line_t ** line);
static int bt_log_new(bt_log_t ** log);
#if 0
static int bt_log_append(bt_log_t * self, bt_log_line_t * line);
#endif
static int bt_log_msgcpy(bt_log_t * self, const char * msg, ssize_t len);
static int bt_log_delete(bt_log_t ** log);
static int bt_test_new(bt_test_t ** test, bt_suite_t * suite, const char * dlname, size_t prefix_len);
static int bt_test_delete(bt_test_t ** test);
static int bt_suite_new(bt_suite_t ** suite, bt_elf_t * elf, const char * dlname, size_t prefix_len);
static int bt_suite_load(bt_suite_t * self, Elf * elf);
static int bt_suite_delete(bt_suite_t ** suite);
static int bt_elf_new(bt_elf_t ** elf, bt_t * butcher, const char * elfname);
static int bt_elf_register_suites(bt_elf_t * self, Elf * elf);
static int bt_elf_load(bt_elf_t * self, Elf * elf);
static int bt_elf_delete(bt_elf_t ** elf);
static int bt_chopper(bt_t * self, bt_test_t * test);

/**
 * logs a message to the write end of the pipe in the tester object
 *
 * @param[in] tester the tester object
 * @param[in] filename should be "__FILE__" for most cases
 * @param[in] function should be "__FUNCTION__" for most cases
 * @param[in] line should be "__LINE__" for most cases
 * @param[in] fmt vdprintf a format string
 * @param[in] ... remaining arguments to vdprintf
 */
void bt_log_function(bt_tester_t * tester, const char *filename, const char *function, int line, char *fmt, ...)
{
	va_list argp;
	UNUSED_PARAM(function);

	if (!tester)
		return;

	va_start(argp, fmt);

	dprintf(tester->pipefd[1], "%s:%s:%d: ", filename, function, line);
	vdprintf(tester->pipefd[1], fmt, argp);

	va_end(argp);


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

	if (len <= 0)
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
 * creates a new test for a given suite which is is intended to run function
 * indicated by dlname
 *
 * @param[out] test a pointer to a pointer to hold the test
 * @param[in] suite the suite the new test should belong to
 * @param[in] dlname the function symbol name to pass to dlsym
 * @param[in] prefix_len the length of the suite prefix needed to derive the name for this test from
 *
 * @return the operation error code
 */

int bt_test_new(bt_test_t ** test, bt_suite_t * suite, const char * dlname, size_t prefix_len)
{
	bt_test_t * self;
	int err;
	char * str;
	size_t length;
	char * name;
	void * ptr;

	if (!test)
		return_error(EINVAL);

	self = malloc(sizeof(bt_test_t));
	if (!self)
		return_error(ENOMEM);

	memset(self, 0, sizeof(bt_test_t));

	self->next = NULL;
	memset(self->results, BT_TEST_NONE, BT_PASS_MAX);
	self->log = NULL;

	self->suite = suite;

	self->name = strdup(dlname + prefix_len);
	if (!self->name) {
		err = ENOMEM;
		goto failure;
	}

	length = strlen(dlname) + 30;
	str = alloca(length);
	snprintf(str, length, "%s_descr", dlname);

	name = dlsym(self->suite->elf->dlhandle, str);
	if (name) {
		self->description = strdup(name);
		if (!name) {
			err = ENOMEM;
			goto failure;
		}
	}

	ptr = dlsym(self->suite->elf->dlhandle, dlname);
	if (!ptr) {
		err = EINVAL;
		goto failure;
	}
	*(void **)(&self->function) = ptr;

	*test = self;

	return 0;

	failure: {
		bt_test_delete(&self);
		return_error(err);
	}
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
	free(self->description);
	bt_log_delete(&self->log);

	free(self);

	return 0;
}

/**
 * creates a new test suite which will hold tests corresponding to this suite
 *
 * @param[out] suite a pointer to a pointer to hold the suite
 * @param[in] elf the shared object in which contains the suite an its tests
 * @param[in] dlname the char array symbol name to pass to dlsym that defines the suite
 * @param[in prefix_len the length of the elf prefix needed to derive the name for this suite from
 *
 * @return the operation error code
 */
int bt_suite_new(bt_suite_t ** suite, bt_elf_t * elf, const char * dlname, size_t prefix_len)
{
	bt_suite_t * self;
	int err;
	char * str;
	void * ptr;
	size_t length;

	if (!suite)
		return_error(EINVAL);

	self = malloc(sizeof(bt_suite_t));
	if (!self)
		return_error(ENOMEM);

	memset(self, 0, sizeof(bt_suite_t));

	self->next = NULL;
	self->tests = NULL;

	self->elf = elf;
	self->name = strdup(dlname + prefix_len);
	if (!self->name) {
		err = ENOMEM;
		goto failure;
	}

	str = dlsym(self->elf->dlhandle, dlname);
	if (!str) {
		err = EINVAL;
		goto failure;
	}
	self->description = strdup(str);
	if (!self->description) {
		err = ENOMEM;
		goto failure;
	}

	length = strlen(dlname) + 30;
	str = alloca(length);

	snprintf(str, length, "__bt_suite_%s_setup", self->name);
	ptr = dlsym(self->elf->dlhandle, str);
	*(void **)(&self->setup) = ptr;

	snprintf(str, length, "__bt_suite_%s_teardown", self->name);
	ptr = dlsym(self->elf->dlhandle, str);
	*(void **)(&self->teardown) = ptr;

	*suite = self;

	return 0;

	failure: {
		bt_suite_delete(&self);
		return_error(err);

	}
}

/**
 * loads tests into a given suite by consulting libelf
 *
 * @param[in] self a pointer to a suite
 * @param[in] elf a libelf descriptor for the shared object
 *
 * @return the operation error code
 */
int bt_suite_load(bt_suite_t * self, Elf * elf)
{
	int i, count;
	const char * name;
	Elf_Scn * scn = NULL;
	Elf_Data * edata = NULL;
	GElf_Sym sym;
	GElf_Shdr shdr;
	int err = 0;
	size_t length, cmplen;
	bt_test_t * test, * last;

	if (!self || !elf)
		return_error(EINVAL);

	length = strlen(self->name) + 30;
	char str[length];

	last = NULL;

	snprintf(str, length, "__bt_suite_%s_test_", self->name);
	cmplen = strlen(str);

	scn = elf_getscn(elf, 0);
	while (!err && scn != NULL) {
		gelf_getshdr(scn, &shdr);
		if(shdr.sh_type == SHT_SYMTAB) {
			edata = elf_getdata(scn, NULL);
			if (!edata)
				continue;

			count = shdr.sh_size / shdr.sh_entsize;
			for (i = 0; i < count; i++) {
				gelf_getsym(edata, i, &sym);

				/* ELF64_ST_TYPE and ELF32_ST_TYPE are equal */
				if (ELF64_ST_TYPE(sym.st_info) == STT_FUNC) {
					name = elf_strptr(elf, shdr.sh_link, sym.st_name);
					if (!name)
						continue;
					if (strncmp(name, str, cmplen) == 0) {

						if (regexec(&self->elf->butcher->tregex, name + cmplen, 0, NULL, 0))
							continue;

						test = NULL;
						err = bt_test_new(&test, self, name, cmplen);
						if (err)
							goto failure;

						if (last) {
							last->next = test;
							last = test;
						} else {
							self->tests = test;
							last = test;
						}
					}
				}

			}
		}
		scn = elf_nextscn(elf, scn);
	}

	return 0;

	failure: {
		if (test)
			bt_test_delete(&test);
		return_error(err);
	}
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

	if (!suite || !*suite)
		return_error(EINVAL);

	self = *suite;

	bt_test_t * cur, * tmp;
	cur = self->tests;
	while (cur) {
		tmp = cur->next;
		bt_test_delete(&cur);
		cur = tmp;
	}
	free(self->name);
	free(self->description);

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
	self->suites = NULL;

	self->butcher = butcher;

	self->name = strdup(elfname);
	if (!self->name) {
		err = ENOMEM;
		goto failure;
	}

	self->dlhandle = dlopen(self->name, RTLD_LAZY);
	if (!self->dlhandle) {
		dprintf(self->butcher->fd, "%s\n", dlerror());
		err = ENFILE;
		goto failure;
	}
	dprintf(self->butcher->fd, "%s\n", dlerror());

	*elf = self;

	return 0;

	failure: {
		bt_elf_delete(&self);
		return_error(err);
	}
}

/**
 * iterates registered suites and creates corresponding tests
 *
 * @param[in] self a pointer to an elf descriptor
 * @param[in] elf a pointer to a libelf descriptor
 *
 * @return the operation error code
 */
int bt_elf_register_suites(bt_elf_t * self, Elf * elf)
{
	bt_suite_t * cur, * tmp;
	int err = 0;

	if (!self || !elf)
		return_error(EINVAL);

	cur = self->suites;
	while (cur) {
		tmp = cur->next;

		err = bt_suite_load(cur, elf);
		if (err)
			return_error(err);

		cur = tmp;
	}

	return 0;
}

/**
 * iterates the libelf descriptor and creates empty test suites from definitions
 *
 * @param[in] self  a pointer to an elf descriptor
 * @param[in] elf a pointer to a libelf descriptor
 *
 * @return the operation error code
 */
int bt_elf_load(bt_elf_t * self, Elf * elf)
{
	int i, count;
	const char * name;
	Elf_Scn * scn = NULL;
	Elf_Data * edata = NULL;
	GElf_Sym sym;
	GElf_Shdr shdr;
	bt_suite_t * suite;
	int err = 0;

	if (!self || !elf)
		return_error(EINVAL);



	scn = elf_getscn(elf, 0);
	while (!err && scn != NULL) {
		gelf_getshdr(scn, &shdr);
		if(shdr.sh_type == SHT_SYMTAB) {
			edata = elf_getdata(scn, NULL);
			if (!edata)
				continue;

			count = shdr.sh_size / shdr.sh_entsize;
			for (i = 0; i < count; i++) {
				gelf_getsym(edata, i, &sym);

				/* ELF64_ST_TYPE and ELF32_ST_TYPE are equal */
				if (ELF64_ST_TYPE(sym.st_info) == STT_OBJECT) {
					name = elf_strptr(elf, shdr.sh_link, sym.st_name);
					if (!name)
						continue;
					if (strncmp(name, "__bt_suite_def_", 15) == 0) {
						if (regexec(&self->butcher->sregex, name + 15, 0, NULL, 0))
							continue;
						suite = NULL;
						err = bt_suite_new(&suite, self, name, 15);
						if (err)
							goto failure;

						suite->next = self->suites;
						self->suites = suite;
					}
				}

			}
		}
		scn = elf_nextscn(elf, scn);
	}

	err = bt_elf_register_suites(self, elf);
	if (err)
		goto failure;

	return 0;

	failure: {
		if (suite)
			bt_suite_delete(&suite);
		return_error(err);
	}
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

	if (!elf || !*elf)
		return_error(EINVAL);

	self = *elf;

	bt_suite_t * cur, * tmp;
	cur = self->suites;
	while (cur) {
		tmp = cur->next;
		bt_suite_delete(&cur);
		cur = tmp;
	}

	if (self->dlhandle)
		dlclose(self->dlhandle);

	free(self->name);

	free(self);

	return 0;
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
 * @param[in] self a pointer the butcher
 * @param[in] smatch a regex string used to select test suites
 * @param[in] tmatch a regex string used to select tests
 *
 * @return the operation error code
 */
int bt_init(bt_t * self, int fd, const char * smatch, const char * tmatch)
{
	if (!self)
		return_error(EINVAL);

	if (elf_version(EV_CURRENT) == EV_NONE) {
		return_error(EINVAL);
	}

	self->fd = fd;

	if (smatch) {
		if(regcomp(&self->sregex, smatch, REG_EXTENDED | REG_NOSUB))
			return_error(EINVAL);
	} else { /* grab all suites by default */
		if (regcomp(&self->sregex, ".*", REG_EXTENDED | REG_NOSUB))
			return_error(EINVAL);
	}

	if (tmatch) {
		if(regcomp(&self->tregex, tmatch, REG_EXTENDED | REG_NOSUB))
			return_error(EINVAL);
	} else { /* grab all tests by default */
		if (regcomp(&self->tregex, ".*", REG_EXTENDED | REG_NOSUB))
			return_error(EINVAL);
	}

	self->initialized = 1;

	return 0;
}

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

	return 0;
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
int bt_loadv(bt_t * self, int paramc, char * paramv[]) {
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
	int fd;
	int err;
	Elf * elf;
	bt_elf_t * btelf;

	if (!self || !self->initialized || !elfname)
		return_error(EINVAL);

	fd = open(elfname, O_RDONLY);
	if (!fd) {
		dprintf(self->fd, "could not open file '%s'\n", elfname);
		return_error(errno);
	}

	elf = elf_begin(fd, ELF_C_READ, NULL);
	if (!elf) {
		dprintf(self->fd, "could not create a libelf handle for file '%s'\n", elfname);
		return_error(EINVAL);
	}

	btelf = NULL;
	err = bt_elf_new(&btelf, self, elfname);
	if (err) /* allocation error? */
		goto failure;

	err = bt_elf_load(btelf, elf);
	if (err)
		goto failure;

	elf_end(elf);
	close(fd);

	btelf->next = self->elfs;
	self->elfs = btelf;

	return 0;

	failure: {
		if (btelf)
			bt_elf_delete(&btelf);
		elf_end(elf);
		close(fd);
		return_error(err);
	}
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
	bt_elf_t *elf_cur;
	bt_suite_t *suite_cur;
	bt_test_t *test_cur;

	if (!self || !self->initialized)
		return_error(EINVAL);

	dprintf(self->fd, "%slisting loaded objects%s...\n",
			self->color?"\x1b[1;32m":"", self->color?"\x1b[0m":"");

	elf_cur = self->elfs;
	while (elf_cur) {
		dprintf(self->fd, "[%self%s, name='%s%s%s', dlhandle=%p]\n",
				self->color?"\x1b[1;33m":"", self->color?"\x1b[0m":"",
				self->color?"\x1b[1;31m":"", elf_cur->name, self->color?"\x1b[0m":"",
				elf_cur->dlhandle);

		suite_cur = elf_cur->suites;
		while (suite_cur) {
			dprintf(self->fd, " [%ssuite%s, name='%s%s%s', description='%s%s%s', setup=%p, teardown=%p]\n",
					self->color?"\x1b[1;34m":"", self->color?"\x1b[0m":"",
					self->color?"\x1b[1;32m":"", suite_cur->name, self->color?"\x1b[0m":"",
					self->color?"\x1b[1;35m":"", suite_cur->description, self->color?"\x1b[0m":"",
					*(void **)(&suite_cur->setup), *(void **)(&suite_cur->teardown));

			test_cur = suite_cur->tests;
			while (test_cur) {
				dprintf(self->fd, "  [%stest%s, name='%s%s%s', description='%s%s%s', function=%p]\n",
						self->color?"\x1b[1;35m":"", self->color?"\x1b[0m":"",
						self->color?"\x1b[1;31m":"", test_cur->name, self->color?"\x1b[0m":"",
						self->color?"\x1b[1;33m":"", test_cur->description, self->color?"\x1b[0m":"",
						*(void **)(&test_cur->function));

				test_cur = test_cur->next;
			}

			suite_cur = suite_cur->next;
		}

		elf_cur = elf_cur->next;
	}

	return 0;
}

/**
 * internal function that runs a test
 *
 * @param[in] self a pointer the butcher
 * @param[in] test the test to run
 *
 * @return the operation error code
 */
int bt_chopper(bt_t * self, bt_test_t * test)
{
	int status;
	pid_t pid;
	bt_tester_t tester;
	int err;

	struct result_rec {
		char magic[5];
		char results[BT_PASS_MAX];
		char done;
	};

	int pipefd[2], pipetmpfd[2];

	if (!self || !self->initialized || !test)
		return_error(EINVAL);

	err = bt_log_new(&test->log);
	if (err)
		return_error(err);

	pipe2(tester.pipefd, O_NONBLOCK);
	tester.setup = test->suite->setup;
	tester.function = test->function;
	tester.teardown = test->suite->teardown;

	pid = fork();
	if (pid == -1)
		return_error(ENAVAIL);
	else if (pid == 0) {
		/* forked here */
	} else {
		ssize_t length, bytes;
		char * buf, c;
		ssize_t i, n;
		err = 0;
		length = 0;
		bytes = 0;
		struct result_rec rec = {"\x01\x02\x03\x04", {BT_TEST_NONE, BT_TEST_NONE, BT_TEST_NONE}, 0};

		close(tester.pipefd[1]);
		if (waitpid(pid, &status, 0) == -1)
			goto out;

		/* Linus' way to do the right thing?! */
		err = ioctl(tester.pipefd[0], FIONREAD, &bytes);
		if (err) {
			err = errno;
			goto out;
		}

		if (bytes == 0)
			bytes = 512;

		buf = malloc(bytes + 1);
		if (!buf) {
			err = ENOMEM;
			goto out;
		}

		length = read(tester.pipefd[0], buf, bytes);
		buf[length] = '\0';

		i = 0;
		while (i < length) {
			for (n = i; n < length && (c = buf[n]) != '\n' && c != '\0'; n++);
			switch (c) {
				case '\n':
					err = bt_log_msgcpy(test->log, buf + i, n - i);
					if (err)
						goto out;
					i = n + 1;
					break;
				default:
					if (length > i && length- i > (ssize_t)sizeof(struct result_rec)) {
						if (strcmp(buf + i + 1, rec.magic) == 0) {
							memcpy(&rec, buf + i + 1, sizeof(struct result_rec));
						}
					}
					i = n + sizeof(struct result_rec) + 1;
					break;
			}
		}


		free(buf);
		close(tester.pipefd[0]);

		if (WIFEXITED(status)) {
			if (!rec.done)
				return_error(EINVAL);

			for (int i = 0; i < BT_PASS_MAX; i++) {
				test->results[i] = rec.results[i];
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

		} else
			goto out;

		return 0;

		out: {
			if (buf)
				free(buf);
			close(tester.pipefd[0]);
			return_error(err);
		}
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
	bt_elf_t *elf_cur;
	bt_suite_t *suite_cur;
	bt_test_t *test_cur;
	int err;



	if (!self || !self->initialized)
		return_error(EINVAL);

	elf_cur = self->elfs;
	while (elf_cur) {
		suite_cur = elf_cur->suites;
		while (suite_cur) {
			test_cur = suite_cur->tests;
			while (test_cur) {
				err = bt_chopper(self, test_cur);
				if (err)
					return_error(err);

				test_cur = test_cur->next;
			}

			suite_cur = suite_cur->next;
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
	bt_elf_t *elf_cur;
	bt_suite_t *suite_cur;
	bt_test_t *test_cur;
	bt_log_line_t * line_cur;

	if (!self || !self->initialized)
		return_error(EINVAL);

	unsigned int allresults[BT_TEST_MAX] = { 0 };
	int allcount = 0;

	dprintf(self->fd, "%slisting results for loaded objects%s (worst counts)...\n",
			self->color?"\x1b[1;32m":"", self->color?"\x1b[0m":"");

	elf_cur = self->elfs;
	while (elf_cur) {
		dprintf(self->fd, "[%self%s, name='%s%s%s']\n",
				self->color?"\x1b[1;33m":"", self->color?"\x1b[0m":"",
				self->color?"\x1b[1;31m":"", elf_cur->name, self->color?"\x1b[0m":"");

		suite_cur = elf_cur->suites;
		while (suite_cur) {
			dprintf(self->fd, " [%ssuite%s, name='%s%s%s', description='%s%s%s']\n",
					self->color?"\x1b[1;34m":"", self->color?"\x1b[0m":"",
					self->color?"\x1b[1;32m":"", suite_cur->name, self->color?"\x1b[0m":"",
					self->color?"\x1b[1;35m":"", suite_cur->description, self->color?"\x1b[0m":"");

			unsigned int results[BT_TEST_MAX] = { 0 };
			int result;
			int count = 0;

			test_cur = suite_cur->tests;
			while (test_cur) {
				result = BT_TEST_NONE;
				for (int i = 0; i < BT_PASS_MAX; i++) {
					if (test_cur->results[i] > result)
						result = test_cur->results[i];
				}

				if (self->verbose || result > BT_TEST_SUCCEEDED) {
					dprintf(self->fd, "  [%stest%s, name='%s%s%s', description='%s%s%s']\n",
							self->color?"\x1b[1;35m":"", self->color?"\x1b[0m":"",
							self->color?"\x1b[1;31m":"", test_cur->name, self->color?"\x1b[0m":"",
							self->color?"\x1b[1;33m":"", test_cur->description, self->color?"\x1b[0m":"");
				}

				if (test_cur->log) {
					line_cur = test_cur->log->lines;
					while (line_cur) {
						if (result > BT_TEST_SUCCEEDED) {
							dprintf(self->fd, "   %s%s%s\n",
									self->color?"\x1b[1;31m":"", line_cur->contents, self->color?"\x1b[0m":"");
						} else {
							dprintf(self->fd, "   %s\n", line_cur->contents);
						}
						line_cur = line_cur->next;
					}
				}

				if ((self->verbose && result > BT_TEST_NONE) || result > BT_TEST_SUCCEEDED)
					dprintf(self->fd, "   -> results: ");

				if (self->verbose || result > BT_TEST_SUCCEEDED) {
					for (int i = 0, flag = 0; i < BT_PASS_MAX; i++) {
						if (test_cur->results[i] > BT_TEST_NONE) {
							if (flag)
								dprintf(self->fd, ", ");
							else
								flag = 1;

							switch (i) {
								case BT_PASS_SETUP:
									dprintf(self->fd, "%ssetup%s ", self->color?"\x1b[1;36m":"", self->color?"\x1b[0m":""); break;
								case BT_PASS_TEST:
									dprintf(self->fd, "%stest%s ", self->color?"\x1b[1;36m":"", self->color?"\x1b[0m":""); break;
								case BT_PASS_TEARDOWN:
									dprintf(self->fd, "%steardown%s ", self->color?"\x1b[1;36m":"", self->color?"\x1b[0m":""); break;
								default:
									break;
							}

							switch (test_cur->results[i]) {
								case BT_TEST_SUCCEEDED:
									dprintf(self->fd, "%ssucceeded%s", self->color?"\x1b[1;32m":"", self->color?"\x1b[0m":""); break;
								case BT_TEST_IGNORED:
									dprintf(self->fd, "%signored%s", self->color?"\x1b[1;33m":"", self->color?"\x1b[0m":""); break;
								case BT_TEST_FAILED:
									dprintf(self->fd, "%sfailed%s", self->color?"\x1b[1;31m":"", self->color?"\x1b[0m":""); break;
								case BT_TEST_CORRUPTED:
									dprintf(self->fd, "%scorrupted%s", self->color?"\x1b[1;31m":"", self->color?"\x1b[0m":""); break;
								default:
									break;
							}
						}
					}
				}

				if (self->verbose || result > BT_TEST_SUCCEEDED) {
					switch (result) {
						case BT_TEST_SUCCEEDED:
							dprintf(self->fd, " -> [%ssucceeded%s]\n", self->color?"\x1b[1;46m\x1b[2;32m":"", self->color?"\x1b[0m":""); break;
						case BT_TEST_IGNORED:
							dprintf(self->fd, " -> [%signored%s]\n", self->color?"\x1b[1;46m\x1b[2;33m":"", self->color?"\x1b[0m":""); break;
						case BT_TEST_FAILED:
							dprintf(self->fd, " -> [%sfailed%s]\n", self->color?"\x1b[1;41m":"", self->color?"\x1b[0m":""); break;
						case BT_TEST_CORRUPTED:
							dprintf(self->fd, " -> [%scorrupted%s]\n", self->color?"\x1b[1;41m":"", self->color?"\x1b[0m":""); break;
						default:
							break;
					}
				}

				if (result > BT_TEST_NONE) {
					results[result]++;
					count++;
				}

				test_cur = test_cur->next;
			}

			for (int i = 0; i < BT_TEST_MAX; i++) {
				allresults[i] += results[i];
			}
			allcount += count;

			dprintf(2, "  => %d tests, %d succeeded (%g%%), %d ignored (%g%%), %d failed (%g%%), %d corrupted (%g%%)\n",
					count, results[BT_TEST_SUCCEEDED], (double)results[BT_TEST_SUCCEEDED] / count * 100,
					results[BT_TEST_IGNORED], (double)results[BT_TEST_IGNORED] / count * 100,
					results[BT_TEST_FAILED], (double)results[BT_TEST_FAILED] / count * 100,
					results[BT_TEST_CORRUPTED], (double)results[BT_TEST_CORRUPTED] / count * 100);

			suite_cur = suite_cur->next;
		}

		elf_cur = elf_cur->next;
	}

	dprintf(2, " => %d tests, %d succeeded (%g%%), %d ignored (%g%%), %d failed (%g%%), %d corrupted (%g%%)\n",
			allcount, allresults[BT_TEST_SUCCEEDED], (double)allresults[BT_TEST_SUCCEEDED] / allcount * 100,
			allresults[BT_TEST_IGNORED], (double)allresults[BT_TEST_IGNORED] / allcount * 100,
			allresults[BT_TEST_FAILED], (double)allresults[BT_TEST_FAILED] / allcount * 100,
			allresults[BT_TEST_CORRUPTED], (double)allresults[BT_TEST_CORRUPTED] / allcount * 100);

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
	
	free(self);

	return 0;
}
