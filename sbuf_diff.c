/*
 * sbuf_diff - structured buffer difference structure
 *
 * Provides a difference structure to compare and distinguish the difference
 * between structured buffers.
 */

#include <stdio.h>              // NULL
#include <stdlib.h>		// malloc(), free()
#include "sbuf.h"
#include "sbuf_diff.h"
#include "llq.h"
#include "llq_num.h"

/**********************************************************/
/*
 * Initializes the given difference structure so that it can be used to compare
 * a set of lines. This function should be called every time before comparing a
 * new set of lines. Returns 0 if successful, or -1 if error.
 */
int sbuf_diff_init(sbuf_diff* d) {
	size_t i;

	// check parameters
	if (d == NULL) {
		return -1;
	}

	// set default values
	d->pos = 0;
	d->cnt = 0;
	for (i = 0; i < d->width; i++) {
		d->cmp[i] = 0;
		d->sub->ptr[i] = 0;
		d->null->ptr[i] = 0;
	}
	d->nbd = 0;

	return 0;
}

/**********************************************************/
/* 
 * Allocates and initializes a new difference structure using the given widht.
 * Returns 0 if successful, or -1 if error.
 */
sbuf_diff* sbuf_diff_malloc(size_t width) {
	sbuf_diff* d;

	// allocate memory for the structure
	d = (sbuf_diff*)malloc(sizeof(sbuf_diff));
	if (d == NULL) {
		return NULL;
	}

	// set the width in the structure
	d->width = width;

	// allocate memory for boolean flags
	d->cmp = (unsigned char*)malloc(sizeof(unsigned char) * width);
	if (d->cmp == NULL) {
		free(d);
		return NULL;
	}

	// allocate memory for subtraction buffer
	d->sub = sbuf_malloc(width);
	if (d->sub == NULL) {
		free(d->cmp);
		free(d);
		return NULL;
	}

	// allocate memory for subtraction buffer
	d->null = sbuf_malloc(width);
	if (d->null == NULL) {
		free(d->sub);
		free(d->cmp);
		free(d);
		return NULL;
	}

	// set default values
	sbuf_diff_init(d);

	return d;
}

/**********************************************************/
/*
 * Frees the memory used by the given difference structure. Returns 0 if
 * successful, or -1 if error.
 */
void sbuf_diff_free(sbuf_diff* d) {

	if (d != NULL) {

		// free null buffer
		if (d->null != NULL) {
			free(d->null);
		}

		// free substraction buffer
		if (d->sub != NULL) {
			free(d->sub);
		}

		// free comparison buffer
		if (d->cmp != NULL) {
			free(d->cmp);
		}

		// free difference structure
		free(d);
	}
}

/**********************************************************/
/*
 * Compares two buffers at the given position and length and updates the given
 * difference structure to indicate which bytes were different, according to
 * the given word size.
 */
int sbuf_diff_cmp(sbuf* sb1, sbuf* sb2, size_t pos, size_t len, sbuf_diff* d, size_t word_size) {
	size_t i;
	size_t j;
	size_t val1;
	size_t val2;
	size_t vald;
	int diff;
	unsigned char* ch1;
	unsigned char* ch2;

	// check parameters
	if (sb1 == NULL || sb2 == NULL) {
		return -1;
	}
	if (d == NULL) {
		return -1;
	}

	// save position
	d->pos = pos;

	// copy position/length to subtraction buffer
	d->sub->pos = pos;
	d->sub->len = len;

	// loop through len bytes according to word size
	for (i = 0; i < len; i += word_size) {

		// initialize values
		val1 = 0;
		val2 = 0;
		diff = 0;

		// loop through word size, limited by len
		for (j = 0; j < word_size; j++) {

			// shift existing values by one byte
			val1 <<= 8;
			val2 <<= 8;

			// past len, skip (but continue shift above)
			if ((i + j) >= len) {
				continue;
			}

			// get first reference character
			ch1 = sbuf_char(sb1, pos + i + j);

			// get second comparison character
			ch2 = sbuf_char(sb2, pos + i + j);

			// check for NULL bytes
			if (ch1 == NULL && ch2 == NULL) {

				// don't mark as different if both NULL

				// track NULL bytes
				d->null->ptr[i + j]++;
			}
			else if (ch1 == NULL && ch2 != NULL) {

				// NULL bytes are compared as different
				if (d->nbd) {
					val2 += *ch2;

					// mark as different
					diff = 1;

					// do NOT track NULL bytes
				}
				else {
					// track NULL bytes
					d->null->ptr[i + j]++;
				}
			}
			else if (ch1 != NULL && ch2 == NULL) {

				// NULL bytes are compared as different
				if (d->nbd) {
					val1 += *ch1;

					// mark as different
					diff = 1;

					// do NOT track NULL bytes
				}
				else {
					// track NULL bytes
					d->null->ptr[i + j]++;
				}
			}
			else {
				// add byte values
				val1 += *ch1;
				val2 += *ch2;

				// mark as different
				if (val1 != val2) {
					diff = 1;
				}
			}
		}

		// update difference structure if marked as different
		if (diff) {

			// calculate the difference
			vald = val2 - val1;

			// increment difference count
			d->cnt += word_size;

			// mark differences and set values
			// according to word size
			for (j = 0; ((j < word_size) && ((i + j) < len)); j++) {
				if (d->cmp[i + j] == 0) {
					d->sub->ptr[i + j] = (vald >> (8 * (word_size - 1 - j))) & 0xff;
				}
				d->cmp[i + j]++;
			}
		}
	}

	return d->cnt;
}

/**********************************************************/
/*
 * Modifies the given difference structure to unmark groups of bytes provided
 * in the given ignore structure, consistent with the word size.
 */
int sbuf_diff_unmark_ignore(sbuf_diff* d, size_t word_size, llq_list* ignore) {
	size_t i;
	size_t j;
	size_t s;
	size_t v;
	llq_node* node;

	// ignore structure must be defined
	if (ignore == NULL) {
		return 0;
	}

	// loop through nodes of ignore structure
	node = ignore->head;
	while (node != NULL) {

		// get value from node in ignore list
		v = llq_num_value(node);

		// loop through difference values
		for (i = 0; i < d->width; i += word_size) {

			// get word size value
			s = sbuf_word(d->sub, d->pos + i, word_size);

			// word size value matches the ignore
			if (s == v) {

				// loop through all bytes in word size
				for (j = i; j < (i + word_size); j++) {

					// reset highlight
					if (d->cmp[j] > 0) {
						d->cnt -= d->cmp[j];
						d->cmp[j] = 0;

						// invalidate diff
						d->sub->ptr[j] = 0;
						d->null->ptr[j] = 0;
					}
				}
			}
		}

		// get next node
		node = node->next;
	}

	return 0;
}

/**********************************************************/
