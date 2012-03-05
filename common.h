/* @@INCLUDE-HEADER@@ */

#ifndef COMMON_H_
#define COMMON_H_

#define BAPI extern

/* error handling */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*#define return_error(_error) return _error*/
#define return_error \
	do { \
		fprintf(stderr, "ERROR %d in %s", _error, __FUNCTION__); \
		return _error; \
	} while(0)

/* flags */

typedef unsigned int flags_t;

#define flags_is_set(flags, bits) (((flags)&(bits)) == (bits))
#define flags_set(flags, bits) ((flags)|(bits))
#define flags_check(flags, bits) ((flags) | (bits)) == (bits)


#define UNUSED_PARAM(x) (void)(x)

#endif /* COMMON_H_ */
