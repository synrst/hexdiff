#ifndef _SBUF_DIFF_H
#define _SBUF_DIFF_H

#include "sbuf.h"
#include "llq.h"

struct sbuf_diff {
	unsigned char* cmp;	// boolean flags
	sbuf* sub;		// buffer for substraction differences
	sbuf* null;		// buffer for NULL differences
	size_t width;		// number of bytes per line per file
	size_t pos;		// relative position
	size_t cnt;		// number of differences found
	int nbd;		// NULL bytes are compared as different
};
typedef struct sbuf_diff sbuf_diff;

int sbuf_diff_init(sbuf_diff* d);
sbuf_diff* sbuf_diff_malloc(size_t width);
void sbuf_diff_free(sbuf_diff* d);
int sbuf_diff_cmp(sbuf* sb1, sbuf* sb2, size_t pos, size_t len, sbuf_diff* d, size_t word_size);
int sbuf_diff_mark_groups(sbuf_diff* d, size_t word_size);
int sbuf_diff_unmark_ignore(sbuf_diff* d, size_t word_size, llq_list* ignore);

#endif /* _SBUF_DIFF_H */
