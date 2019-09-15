/*
 * hexdiff - Display hexadecimal differences between files.
 */

#include <stdio.h>	// printf(), fileno()
#include <stdlib.h>	// exit(), strtoull(), malloc(), free()
#include <string.h>	// strncmp()
#include <fcntl.h>	// open()
#include <sys/stat.h>	// open()
#include <sys/types.h>	// open()
#include <sys/time.h>	// struct timeval, gettimeofday()
#include <unistd.h>	// getopt()
#include "sbuf.h"
#include "sbuf_diff.h"
#include "sbuf_cache.h"
#include "llq.h"
#include "llq_num.h"

#define CODE_VERSION		"0.12"
#define CODE_DATE		"2019-07-06"
#define MAX_FILES		4		// maximum files to load
#define MAX_LENGTH		(size_t)-1	// maximum unsigned length
#define STD_BUF_SIZE		(size_t)262144

// configurable bitwise flags
#define FLAG_COLOR		1		// enable ANSI color
#define FLAG_HEX		2		// enable hexadecimal
#define FLAG_ASCII		4		// enable ascii
#define FLAG_VERBOSE		8		// display all lines
#define FLAG_QUIET1		32		// no names, spacers, etc..
#define FLAG_QUIET2		64		// no positions
#define FLAG_TIME_ELAPSED	128		// display the time elapsed
#define FLAG_DISP_DIFF		256		// display differences
#define FLAG_NULL_BYTES_DIFF	512		// NULLs bytes are different
#define FLAG_UPPER_HEX		1024		// uppercase hexadecimal

// empty spaces
// can change to literal spaces
#define EMPTY_HEX		"XX"
#define EMPTY_ASCII		" "
#define EMPTY_BAR		"|"

// choose color scheme
#define COLOR_SCHEME		0

// color scheme 0
#if COLOR_SCHEME == 0
#define COLOR_RESET		"\x1b[0;0m"
#define COLOR_POS		"\x1b[0;32m"
#define COLOR_STRING		"\x1b[1;37m"
#define COLOR_SPACER		"\x1b[1;37m"
#define COLOR_HEX		"\x1b[0;33m"
#define COLOR_BAR		"\x1b[1;37m"
#define COLOR_ASCII		"\x1b[0;35m"
#define COLOR_HEX_HL		"\x1b[1;33;44m"
#define COLOR_ASCII_HL		"\x1b[1;37;41m"
// color scheme 1
#elif COLOR_SCHEME == 1
#define COLOR_RESET		"\x1b[0;0m"
#define COLOR_POS		"\x1b[0;37m"
#define COLOR_STRING		"\x1b[1;37m"
#define COLOR_SPACER		"\x1b[1;37m"
#define COLOR_HEX		"\x1b[0;35m"
#define COLOR_BAR		"\x1b[1;32m"
#define COLOR_ASCII		"\x1b[0;34m"
#define COLOR_HEX_HL		"\x1b[1;41;33m"
#define COLOR_ASCII_HL		"\x1b[1;41;33m"
#endif

/**********************************************************/
/*
 * Prints a usage statement to STDERR.
 */
void usage(char* program, char* error) {
	fprintf(stderr, "hexdiff %s released %s\n", CODE_VERSION, CODE_DATE);
	fprintf(stderr, "Usage: %s [options] FILE [...]\n", program);
	fprintf(stderr, "Display hexadecimal differences between files.\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "    -v         : verbose, display all lines\n");
	fprintf(stderr, "    -q         : quiet, do not display file names, spacers, bytes, bars\n");
	fprintf(stderr, "    -Q         : quiet, do not display position\n");
	fprintf(stderr, "    -n         : disable ANSI color\n");
	fprintf(stderr, "    -d         : display differences (file 1 subtract file 0 only)\n");
	fprintf(stderr, "    -H         : display hexadecimal only\n");
	fprintf(stderr, "    -A         : display ASCII only\n");
	fprintf(stderr, "    -N         : NULL bytes are compared as different\n");
	fprintf(stderr, "    -t         : display the time elapsed to STDERR\n");
	fprintf(stderr, "    -u         : display hexadecimal in uppercase\n");
	fprintf(stderr, "    -p offset  : sets the display offset position (default is 0)\n");
	fprintf(stderr, "    -l length  : sets the maximum length to display (default is until EOF)\n");
	fprintf(stderr, "    -w width   : sets the number of bytes per line (default is 16)\n");
	fprintf(stderr, "    -h width   : sets the number of differing bytes to highlight (default is 1)\n");
	fprintf(stderr, "    -c context : sets the number of lines of context (default is 0)\n");
	fprintf(stderr, "    -s #:seek  : seeks to offset position of file # (starting at 0)\n");
	fprintf(stderr, "    -S #:shift : shifts starting offset position for file # (starting at 0)\n");
	fprintf(stderr, "    -X #       : excludes output for file # (starting at 0)\n");
	fprintf(stderr, "    -I diff    : ignore the given difference, based on -h (default is none)\n");
	fprintf(stderr, "    -b size    : sets the I/O buffer size (default is ");
	fprintf(stderr, "%zu", STD_BUF_SIZE);
	fprintf(stderr, ")\n");
	fprintf(stderr, "    -?         : display this help message\n");
	if (error != NULL) {
		fprintf(stderr, "\nERROR: %s\n", error);
	}
	exit(EXIT_FAILURE);
}

/**********************************************************/
/*
 * Parses a string and returns the numerical representation as an unsigned
 * value. Supports both decimal and hexadecimal values.
 */
size_t parse_value(const char* str) {
	size_t offset;

	if (str == NULL) {
		offset = 0;
	}
	else if (strncmp(str, "0x", 2) == 0) {
		offset = strtoull(str + 2, 0, 16);
	}
	else {
		offset = strtoull(str, 0, 10);
	}

	return offset;
}

/**********************************************************/
/*
 * Returns the amount of time elapsed in seconds between the given time
 * structures. The t1 structure should occur after the t0 structure.
 */
double time_elapsed(struct timeval t1, struct timeval t0) {
	return (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1000000.0f;
}

/**********************************************************/
/*
 * Returns the number of spaces associated with each line of a file on the
 * screen based on the given width of data bytes to print and the flags.
 */
int wspaces(size_t width, int flags) {
	int spaces = 0;

	// NOTE: extra space before hex is not counted here

	// number of spaces for hex values
	if (flags & FLAG_HEX) {
		spaces += (width * 2);

		// spaces between hex groups
		spaces += (width / 4) - 1;
		spaces += (width % 4 > 0 ? 1 : 0);
	}

	// extra space between hex and ascii
	if ((flags & FLAG_HEX) && (flags & FLAG_ASCII)) {
		spaces += 1;
	}

	// number of spaces for ascii values
	if (flags & FLAG_ASCII) {
		spaces += width;

		// bars around ascii
		if (! (flags & FLAG_QUIET1) && (flags & FLAG_ASCII)) {
			spaces += 2;
		}
	}

	return spaces;
}

/**********************************************************/
/*
 * Prints a new line. Always returns 0.
 */
int print_nl(int flags) {
	printf("\n");
	return 0;
}

/**********************************************************/
/*
 * Prints a position. If the COLOR flag is set, ANSI color codes will be
 * printed. Always returns 0.
 */
int print_pos(size_t pos, int flags) {

	if (flags & FLAG_QUIET2) {
		return 0;
	}

	// print value in hex
	if (flags & FLAG_COLOR) {
		printf(COLOR_POS);
		if (flags & FLAG_UPPER_HEX) {
			printf("%08zX", pos);
		}
		else {
			printf("%08zx", pos);
		}
		printf(COLOR_RESET);
	}
	else {
		if (flags & FLAG_UPPER_HEX) {
			printf("%08zX", pos);
		}
		else {
			printf("%08zx", pos);
		}
	}

	return 0;
}

/**********************************************************/
/*
 * Prints empty spaces where a position would normally reside. Calculates the
 * number of spaces to print for positions larger than 32 bits, otherwise
 * prints 8 spaces. Always returns 0.
 */
int print_empty_pos(size_t pos, int flags) {
	unsigned char ch_len;

	if (flags & FLAG_QUIET2) {
		return 0;
	}

	// determine length of hex value
	ch_len = 0;
	if (pos == 0) {
		ch_len = 8;
	}
	else {
		while (pos >> (4*ch_len) != 0) {
			ch_len++;
		}
	}
	if (ch_len < 8) {
		ch_len = 8;
	}

	// print determined number of spaces
	printf("%*s", ch_len, "");

	return 0;
}

/**********************************************************/
/*
 * Prints a NULL terminated string padded with spaces up to span bytes. If the
 * string is longer than span, it will be truncated. If the COLOR flag is set,
 * ANSI color codes will be printed. Always returns 0;
 */
int print_string(char* str, int span, int flags) {

	// extra space
	if (! (flags & FLAG_QUIET1)) {
		printf(" ");
	}

	// NULL string, print spaces only
	if (str == NULL) {
		printf(" %*s", span, "");
		return 0;
	}

	// print as a string
	if (flags & FLAG_COLOR) {
		printf(COLOR_STRING);
	}
	printf(" %-*.*s", span, span, str);
	if (flags & FLAG_COLOR) {
		printf(COLOR_RESET);
	}

	return 0;
}

/**********************************************************/
/*
 * Prints an unsigned number padded with spaces up to span bytes. If the
 * resulting string is longer than span, it will be truncated. If the COLOR
 * flag is set, ANSI color codes will be printed. Always returns 0.
 */
int print_bytes(size_t num, int span, int flags) {
	char* buf;

	// allocate memory for temporary string
	buf = (char*)malloc(sizeof(char) * (span + 1));

	// could not allocate memory, print spaces instead
	if (buf == NULL) {
		printf(" %*s", span, "");
		return 0;
	}

	// print as a string
	snprintf(buf, span + 1, "%zu bytes", num);
	print_string(buf, span, flags);

	// free allocated memory
	free(buf);

	return 0;
}

/**********************************************************/
/*
 * Prints a spacer between lines to indicate a gap of lines that did not
 * contain any differences. If the COLOR flag is set, ANSI color codes will be
 * printed. Always returns 0.
 */
int print_spacer(int flags) {

	if (! (flags & FLAG_QUIET1)) {
		if (flags & FLAG_COLOR) {
			printf(COLOR_SPACER);
		}
		printf("*");
		if (flags & FLAG_COLOR) {
			printf(COLOR_RESET);
		}
		print_nl(flags);
	}

	return 0;
}

/**********************************************************/
/*
 * Prints the given buffer of len bytes in both hexadecimal and ASCII. The
 * output will always contain the exact amount of data to equal width bytes,
 * regardless of the size of the buffer. The buffer is padded with NULL bytes
 * at the beginning according to before, and padded with NULL bytes at the end
 * up to width if more than before and len. Highlights specific values in the
 * output according to the difference structure provided, if not NULL. If the
 * COLOR flag is set, ANSI color codes will be printed. If the buffer is
 * end-of-output, len should be set to zero to print NULL bytes for the entire
 * line. Always returns 0.
 */
int print_buf(unsigned char* buf, size_t len, size_t before, size_t width, sbuf_diff* d, int flags) {
	size_t i;
	unsigned char ch;
	int hl;

	// NOTE: buf can be NULL to print empty space
	if (buf == NULL) {
		len = 0;
	}

	// extra space
	if (! (flags & FLAG_QUIET1)) {
		printf(" ");
	}

	// print hex
	if (flags & FLAG_HEX) {

		printf(" ");

		if (flags & FLAG_COLOR) {
			printf(COLOR_HEX);
		}
		for (i = 0; i < width; i++) {

			// separate hex groups
			if (i > 0 && i % 4 == 0) {
				printf(" ");
			}

			if (i < before) {
				printf(EMPTY_HEX);
			}
			else if (i >= len + before) {
				printf(EMPTY_HEX);
			}
			else {
				ch = buf[i - before];

				// mark for highlight
				hl = 0;
				if (d != NULL && d->cmp[i]) {
					hl = 1;
				}

				// print hex character
				if (flags & FLAG_COLOR && hl) {
					printf(COLOR_HEX_HL);
					if (flags & FLAG_UPPER_HEX) {
						printf("%02X", ch);
					}
					else {
						printf("%02x", ch);
					}
					printf(COLOR_HEX);
				}
				else {
					if (flags & FLAG_UPPER_HEX) {
						printf("%02X", ch);
					}
					else {
						printf("%02x", ch);
					}
				}
			}
		}
		if (flags & FLAG_COLOR) {
			printf(COLOR_RESET);
		}
	}

	// print ascii
	if (flags & FLAG_ASCII) {

		printf(" ");

		// print bar
		if (! (flags & FLAG_QUIET1)) {
			if (flags & FLAG_COLOR) {
				printf(COLOR_BAR);
			}
			if (len == 0 || before >= width) {
				printf(EMPTY_BAR);
			}
			else {
				printf("|");
			}
		}

		// print ascii
		if (flags & FLAG_COLOR) {
			printf(COLOR_ASCII);
		}
		for (i = 0; i < width; i++) {
			if (i < before) {
				printf(EMPTY_ASCII);
			}
			else if (i >= len + before) {
				printf(EMPTY_ASCII);
			}
			else {
				ch = buf[i - before];

				// normalize non-printable characters
				if (ch < 32 || ch >= 127) {
					ch = '.';
				}

				// mark for highlight
				hl = 0;
				if (d != NULL && d->cmp[i]) {
					hl = 1;
				}

				// print ascii character
				if (flags & FLAG_COLOR && hl) {
					printf(COLOR_ASCII_HL);
					printf("%c", ch);
					printf(COLOR_ASCII);
				}
				else {
					printf("%c", ch);
				}
			}
		}

		// print bar
		if (! (flags & FLAG_QUIET1)) {
			if (flags & FLAG_COLOR) {
				printf(COLOR_BAR);
			}
			if (len == 0 || before >= width) {
				printf(EMPTY_BAR);
			}
			else {
				printf("|");
			}
		}
	}

	if (flags & FLAG_COLOR) {
		printf(COLOR_RESET);
	}

	return 0;
}

/**********************************************************/
/*
 * Prints the given structured buffer. This is a wrapper for print_buf() and
 * ensures the proper number of NULL bytes are printed according to the given
 * position and width, both before and after any actual data. The maximum line
 * width is used to truncate the length if it is limited due to a user provided
 * option. The difference struct and flags are passed on to print_buf(). Always
 * returns 0.
 */
int print_sbuf(sbuf* sb, size_t pos, size_t width, size_t mlw, sbuf_diff* d, int flags) {
	unsigned char* ptr;
	size_t btp;
	size_t before;

	// print a lines of NULLs for a NULL buffer
	if (sb == NULL) {
		print_buf(NULL, 0, 0, width, d, flags);
		return 0;
	}

	// obtain pointer
	ptr = sbuf_ptr(sb, pos);

	// calculate the number of bytes to print
	btp = sbuf_avail(sb, pos);

	// limit by maximum line width
	if (btp > mlw) {
		btp = mlw;
	}

	// obtain the number of NULL bytes before the buffer
	before = sbuf_before(sb, pos);
	if (before > 0 && btp >= before) {
		btp -= before;
	}

	// print buffer
	print_buf(ptr, btp, before, width, d, flags);

	return 0;
};

/**********************************************************/
/*
 * Prints the given difference structure. This is a wrapper for print_buf() and
 * ensures the proper number of NULL bytes are printed according to the given
 * position and width, and other values within the difference structure. The
 * maximum line width is used to truncate the length if it is limited due to a
 * user provided option. The difference structure itself is NOT passed on to
 * print_buf() (it should not be highlighted), but the flags are passed on.
 */
int print_diff(sbuf_diff* d, size_t pos, size_t width, size_t mlw, int flags) {
	unsigned char* ptr;
	size_t btp;
	size_t i;
	size_t before;
	sbuf* sb;

	// print a line of NULLs for a NULL buffer
	if (d == NULL) {
		print_buf(NULL, 0, 0, width, NULL, flags);
		return 0;
	}
	sb = d->sub;
	if (sb == NULL) {
		print_buf(NULL, 0, 0, width, NULL, flags);
		return 0;
	}

	// obtain pointer
	ptr = sbuf_ptr(sb, pos);

	// calculate the number of bytes to print
	btp = sbuf_avail(sb, pos);

	// limit by maximum line width
	if (btp > mlw) {
		btp = mlw;
	}

	// check number of NULL bytes before the buffer
	before = 0;
	for (i = 0; i < width; i++) {

		// NULL bytes and difference of 0
		if (d->null->ptr[i] > 0 && d->sub->ptr[i] == 0) {
			before++;
			ptr += 1;
			btp -= 1;
		}

		// non-differing bytes (before only)
		else if (d->cmp[i] == 0) {
			before++;
			ptr += 1;
			btp -= 1;
		}

		else {
			break;
		}
	}

	// check number of NULL bytes after the buffer in reverse
	for (i = width - 1; 0 <= i && i <= width && btp > 0; i--) {

		// NULL bytes and difference of 0
		if (d->null->ptr[i] > 0 && d->sub->ptr[i] == 0) {
			btp -= 1;
		}

		// non-differing bytes (after only)
		else if (d->cmp[i] == 0) {
			btp -= 1;
		}

		else {
			break;
		}
	}

	// do NOT highlight differences, pass NULL instead of d
	print_buf(ptr, btp, before, width, NULL, flags);

	return 0;
};

/**********************************************************/
/*
 * Adds the given buffer to the given cache at the given position. The buffer
 * must already be filled and contain data at the given position. The cache
 * is limited by the given maximum number of lines. In such a case, a line is
 * removed from the cache before the new line is added. Returns 0 if
 * successful, or -1 if error.
 */
int cache_add(sbuf* sb, sbuf_cache* c, size_t pos, size_t max_lines) {
	int removed = 0;
	unsigned char* sb_ptr;
	size_t sb_before;
	size_t sb_rpos;
	size_t sb_len;

	// check parameters
	if (sb == NULL || c == NULL) {
		return -1;
	}

	// cache is maxed out
	while (c->active.size >= max_lines) {

		// discard one line from cache
		sbuf_cache_remove(c);

		// increment flag for return value
		removed++;
	}

	// obtain values to add to cache
	sb_ptr = sbuf_ptr(sb, pos);
	sb_before = sbuf_before(sb, pos);
	sb_rpos = sbuf_rpos(sb, pos);
	sb_len = sbuf_len(sb, pos);

	// buffer is past position
	if (sb_rpos > (pos + c->max_buf_len)) {

		// add to cache, but with 0 bytes
		if (sbuf_cache_append(c, NULL, pos, 0) == NULL) {
			return -1;
		}

		return removed;
	}

	// position is included in buffer, but preceded by NULLS
	else if (sb_rpos > pos) {

		// truncate length
		if (sb_before <= c->max_buf_len) {
			sb_len = c->max_buf_len - sb_before;
		}
	}

	// truncate length to cache size
	if (sb_len > c->max_buf_len) {
		sb_len = c->max_buf_len;
	}

	// add to cache
	if (sbuf_cache_append(c, sb_ptr, sb_rpos, sb_len) == NULL) {
		return -1;
	}

	return removed;
}

/**********************************************************/

int main(int argc, char* argv[]) {

	// file variables
	int file_cnt = 0;
	char* filename[MAX_FILES];
	sfile* sf[MAX_FILES];
	sbuf* sb[MAX_FILES];
	sbuf_cache* cache[MAX_FILES];
	int f_excl[MAX_FILES];
	llq_list* ignore = NULL;
	sbuf_diff* diff;

	// configurable variables
	size_t width = 16;
	size_t start_pos = 0;
	size_t len = MAX_LENGTH;
	size_t end_pos = MAX_LENGTH;
	size_t hl_width = 1;
	size_t context = 0;
	size_t buf_size = STD_BUF_SIZE;
	size_t seek[MAX_FILES];
	size_t shift[MAX_FILES];
	int flags = FLAG_COLOR | FLAG_HEX | FLAG_ASCII;

	// time variables
	struct timeval ts_start, ts_end;

	// tracking variables
	size_t context_after = MAX_LENGTH;
	int spacer_printed = 0;

	// temporary variables
	int i;
	int j;
	char opt;
	size_t tmp;
	int loop;
	size_t pos;		// current position
	ssize_t br;		// bytes read (signed)
	size_t mlw;		// maximum line width
	int eoo_cnt;		// end-of-output count
	int print_line;		// boolean flag to print the current line

	sbuf* tmp_sb;		// temporary sbuf for cache printing
	size_t tmp_pos;		// temporary position for cache printing

	/******************************/

	// elapsed time
	gettimeofday(&ts_start, NULL);

	/******************************/

	// initialize all variables as NULL or zero
	diff = NULL;
	for (i = 0; i < MAX_FILES; i++) {
		filename[i] = NULL;
		sf[i] = NULL;
		sb[i] = NULL;
		cache[i] = NULL;
		seek[i] = 0;
		shift[i] = 0;
		f_excl[i] = 0;
	}

	// allocate ignore buffer
	ignore = llq_num_malloc();
	if (ignore == NULL) {
		usage(argv[0], "Could not allocate ignore buffer.");
	}

	/******************************/

	// command line options
	opterr = 0;
	while ((opt = getopt(argc, argv, "vqQndHANtup:l:w:h:c:s:S:X:I:b:")) != -1) {
		switch(opt) {

			// verbose, display all lines
			case 'v':
				flags |= FLAG_VERBOSE;
				break;

			// quiet, do not display names, spacers, bytes, bars
			case 'q':
				flags |= FLAG_QUIET1;
				break;

			// extra quiet, do not display position
			case 'Q':
				flags |= FLAG_QUIET2;
				break;

			// no color
			case 'n':
				flags ^= FLAG_COLOR;
				break;

			// display and count differences
			case 'd':
				flags |= FLAG_DISP_DIFF;
				break;

			// hex only
			case 'H':
				flags ^= FLAG_ASCII;
				break;

			// ascii only
			case 'A':
				flags ^= FLAG_HEX;
				break;

			// NULL bytes are compared as different
			case 'N':
				flags ^= FLAG_NULL_BYTES_DIFF;
				break;

			// time elapsed
			case 't':
				flags |= FLAG_TIME_ELAPSED;
				break;

			// uppercase hex
			case 'u':
				flags |= FLAG_UPPER_HEX;
				break;

			// output position (offset)
			case 'p':
				start_pos = parse_value(optarg);
				break;

			// length
			case 'l':
				len = parse_value(optarg);
				break;

			// width
			case 'w':
				width = parse_value(optarg);
				break;

			// highlight width
			case 'h':
				hl_width = parse_value(optarg);
				break;

			// context
			case 'c':
				context = parse_value(optarg);
				break;

			// after
			case 's':
				// individual offset
				if (strncmp(optarg + 1, ":", 1) == 0) {
					if (optarg[0] < '0') {
						usage(argv[0], "Bad seek");
					}
					else if (optarg[0] >= MAX_FILES + '0') {
						usage(argv[0], "Bad seek");
					}
					tmp = parse_value(optarg + 2);
					seek[optarg[0] - '0'] = tmp;
				}
				else {
					usage(argv[0], "Bad seek");
				}
				break;

			// before
			case 'S':
				// individual offset
				if (strncmp(optarg + 1, ":", 1) == 0) {
					if (optarg[0] < '0') {
						usage(argv[0], "Bad shift");
					}
					else if (optarg[0] >= MAX_FILES + '0') {
						usage(argv[0], "Bad shift");
					}
					tmp = parse_value(optarg + 2);
					shift[optarg[0] - '0'] = tmp;
				}
				else {
					usage(argv[0], "Bad shift");
				}
				break;

			// exclude
			case 'X':
				tmp = parse_value(optarg);
				if (tmp < MAX_FILES) {
					f_excl[tmp] = 1;
				}
				else {
					usage(argv[0], "Bad file #");
				}
				break;

			// ignore difference
			case 'I':
				tmp = parse_value(optarg);
				llq_num_append(ignore, tmp);
				break;

			// buffer size
			case 'b':
				buf_size = parse_value(optarg);
				break;

			// help
			case '?':
			default:
				usage(argv[0], NULL);
				break;
		}
	}

	// obtain non-option arguments (file names)
	while (optind < argc) {

		// too many
		if (file_cnt >= MAX_FILES) {
			usage(argv[0], "Maximum files exceeded.");
		}

		// check for duplicate STDIN
		if (strncmp(argv[optind], "-", 1) == 0) {
			for (i = 0; i < file_cnt; i++) {
				if (strncmp(filename[i], "-", 1) == 0) {
					usage(argv[0], "Duplicate STDIN.");
				}
			}
		}

		// store name
		filename[file_cnt] = argv[optind];

		// increment number of files
		file_cnt++;

		optind++;
	}

	// verify configuration
	if (file_cnt <= 0) {
		usage(argv[0], "No files specified.");
	}
	if (width <= 0) {
		usage(argv[0], "Illegal argument for width.");
	}
	if (hl_width <= 0 || hl_width > sizeof(size_t)) {
		usage(argv[0], "Illegal argument for highlight width.");
	}
	if (len <= 0) {
		usage(argv[0], "Illegal argument for length.");
	}
	if (context < 0) {
		usage(argv[0], "Illegal argument for context.");
	}
	if (buf_size <= 0) {
		usage(argv[0], "Illegal argument for buffer size.");
	}
	if (buf_size < width) {
		usage(argv[0], "Buffer size cannot be smaller than the width.");
	}
	for (i = 0; i < file_cnt; i++) {
		if ((start_pos + seek[i]) < start_pos) {
			usage(argv[0], "Illegal argument for seek.");
		}
	}
	if (! (flags & (FLAG_HEX | FLAG_ASCII))) {
		usage(argv[0], "Cannot exclude both hexadecimal and ASCII.");
	}

	// initiailize position
	pos = start_pos;

	// adjust maximum length
	if (len == MAX_LENGTH && start_pos > 0) {
		len -= start_pos;
	}

	// check for overflow
	else if ((start_pos + len) < start_pos) {
		len = MAX_LENGTH - start_pos;
	}

	// set end position
	end_pos = start_pos + len;

	/******************************/

	// allocate difference buffer
	diff = sbuf_diff_malloc(width);
	if (diff == NULL) {
		usage(argv[0], "Could not allocate difference buffer.");
	}

	// allocate file buffers
	for (i = 0; i < file_cnt; i++) {

		// allocate file structures
		sf[i] = sfile_malloc();
		if (sf[i] == NULL) {
			usage(argv[0], "Could not allocate file structures.");
		}

		// allocate file buffers
		sb[i] = sbuf_malloc(buf_size);
		if (sb[i] == NULL) {
			usage(argv[0], "Could not allocate file buffers.");
		}

		// allocate cache
		if (context > 0) {
			cache[i] = sbuf_cache_malloc(width);
			if (cache[i] == NULL) {
				usage(argv[0], "Could not allocate cache.");
			}
		}
	}

	// open files
	for (i = 0; i < file_cnt; i++) {
		if (sfile_open(sf[i], filename[i]) != 0) {
			usage(argv[0], "Could not open files.");
		}
	}

	// seek and shift
	for (i = 0; i < file_cnt; i++) {

		// temporary seek position
		tmp = start_pos + seek[i];

		// position is not negated by shift
		if (tmp > shift[i]) {

			// seek
			sfile_seek(sf[i], sb[i], tmp - shift[i]);

			// set new position
			sb[i]->pos = start_pos;
			sf[i]->start_pos = start_pos;
		}
		else {

			// shift
			sfile_shift(sf[i], sb[i], shift[i]);
		}

		// initial file read
		sfile_read(sf[i], sb[i]);
	}

	/******************************/

	if (! (flags & FLAG_QUIET1)) {

		// print spaces in place of position
		print_empty_pos(pos, flags);

		// loop through files
		for (i = 0; i < file_cnt; i++) {

			if (! f_excl[i]) {
				// print file name
				print_string(
					filename[i],
					wspaces(width, flags),
					flags
				);
			}
		}

		printf("\n");
	}

	/******************************/

	// main loop
	loop = 1;
	while (loop) {

		// assume files are not end-of-output
		eoo_cnt = 0;

		// loop through files
		for (i = 0; i < file_cnt; i++) {

			// ensure there is enough data in the buffer
			while (! sf[i]->eof && sbuf_avail(sb[i], pos) < width) {

				// shift/reduce buffer
				sbuf_reduce(sb[i], pos);

				// read more data from file
				br = sfile_read(sf[i], sb[i]);
				if (br < 0) {
					break;
				}
			}

			// end of output
			if (sfile_eoo(sf[i], sb[i], pos) != 0) {
				eoo_cnt++;
			}
		}

		/*****/

		// all files are end-of-output
		if (eoo_cnt == file_cnt) {
			loop = 0;
			break;
		}

		// reset difference structure
		sbuf_diff_init(diff);

		// NULL bytes are compared as different
		if (flags & FLAG_NULL_BYTES_DIFF) {
			diff->nbd = 1;
		}

		// loop doubly through files for comparison
		for (i = 1; i < file_cnt; i++) {
		for (j = 0; j < i; j++) {

			// compare lines
			if (sbuf_diff_cmp(sb[j], sb[i], pos, width, diff, hl_width) > 0) {

				// unmark ignore values
				sbuf_diff_unmark_ignore(diff, hl_width, ignore);
			}
		}
		}

		/*****/

		// determine if the current line should be printed or not
		print_line = 0;

		// always print
		if (flags & FLAG_VERBOSE || file_cnt == 1) {
			print_line = 1;
		}

		// at least one difference
		else if (diff->cnt > 0) {
			print_line = 1;

			// reset context
			context_after = 0;
		}

		// context after a matching line
		else if (context_after < context) {
			print_line = 1;

			// increment context
			context_after++;
		}

		// calculate maximum width of this line according to length
		mlw = width;
		if (pos < end_pos && end_pos < (pos + width)) {
			mlw = end_pos - pos;
		}

		/*****/

		// print current line
		if (print_line) {

			// reset spacer
			spacer_printed = 0;

			// print cache lines first
			// assumes each file has same number of cache entries
			while (context > 0 && cache[0]->active.size > 0) {

				// determine position by counting backwards
				// NOTE: do not use position stored in cache
				tmp_pos = pos - (cache[0]->active.size * width);
				if (tmp_pos > pos) {
					tmp_pos = 0;
				}

				// loop through cache for each file
				for (i = 0; i < file_cnt; i++) {

					// get next entry from cache
					tmp_sb = sbuf_cache_remove(cache[i]);

					// print cache
					if (tmp_sb != NULL && ! f_excl[i]) {

						// print position
						if (i == 0) {
							print_pos(
								tmp_pos,
								flags
							);
						}

						// print cache buffer
						print_sbuf(
							tmp_sb,
							tmp_pos,
							width,
							mlw,
							NULL,
							flags
						);
					}
				}
				printf("\n");
			}

			// print position
			print_pos(pos, flags);

			// loop through files
			for (i = 0; i < file_cnt; i++) {

				if (! f_excl[i]) {
					// print current line of file
					print_sbuf(sb[i],
						pos,
						width,
						mlw,
						diff,
						flags
					);
				}
			}

			// print subtraction differences last
			// only print if there is at least one difference
			// or if the verbose flag is set
			tmp = 0;
			if ((diff->cnt > 0) || (flags & FLAG_VERBOSE)) {
				tmp = 1;
			}
			if (flags & FLAG_DISP_DIFF && tmp) {

				if (! f_excl[i]) {
					// print differences
					print_diff(
						diff,
						pos,
						width,
						mlw,
						flags
					);
				}
			}

			// print newline
			print_nl(flags);
		}

		/*****/

		// do not print line, add to cache
		else if (context > 0) {

			// loop through files
			for (i = 0; i < file_cnt; i++) {

				// update cache
				tmp = cache_add(sb[i], cache[i], pos, context);

				// printer spacer
				if (! spacer_printed && i == 0 && tmp > 0) {
					print_spacer(flags);
					spacer_printed = 1;
				}
			}
		}

		/*****/

		// do not print line, print spacer instead
		else if (! spacer_printed) {
			print_spacer(flags);
			spacer_printed = 1;
		}

		/*****/

		// position overflow
		if ((pos + width) < pos) {
			loop = 0;
		}

		// new position is past specified length
		else if ((pos + width) >= end_pos) {
			loop = 0;
		}

		// increment position for loop
		else {
			pos += width;

			// attempt to re-read from files solely to determine
			// if EOF has been reached, otherwise if last read was
			// the exact size of the buffer there is no way to
			// know if EOF was reached
			for (i = 0; i < file_cnt; i++) {
				sfile_read(sf[i], sb[i]);
			}
		}
	} // main loop

	/******************************/

	if (! (flags & FLAG_QUIET1)) {

		// print spaces in place of last position
		if (pos >= width) {
			print_empty_pos(pos - width, flags);
		}
		else {
			print_empty_pos(0, flags);
		}

		// loop through files
		for (i = 0; i < file_cnt; i++) {

			if (! f_excl[i]) {

				// output was truncated
				if (end_pos > sf[i]->start_pos) {
					tmp = end_pos - sf[i]->start_pos;
				}
				// entire buffer was NULL bytes at beginning
				else {
					tmp = 0;
				}
				// use all bytes read if before end_pos
				if (sf[i]->bytes_read < tmp) {
					tmp = sf[i]->bytes_read;
				}

				// print number of bytes per file
				print_bytes(tmp, wspaces(width, flags), flags);
			}
		}

		printf("\n");
	}

	// data in cache, print final spacer
	// NOTE: will this condition ever hold true?
	// NOTE: should not print the spacer after the number of bytes
	if (context > 0 && cache[0]->active.size > 0) {
		if (! spacer_printed) {
			print_spacer(flags);
			spacer_printed = 1;
		}
	}

	/*****/

	// close files and free buffers
	llq_num_free(ignore);
	sbuf_diff_free(diff);
	for (i = 0; i < file_cnt; i++) {
		sfile_close(sf[i]);
		sfile_free(sf[i]);
		sbuf_free(sb[i]);
		sbuf_cache_free(cache[i]);
	}

	// print elapsed time
	if (flags & FLAG_TIME_ELAPSED) {
		gettimeofday(&ts_end, NULL);
		fprintf(stderr, "%f seconds\n", time_elapsed(ts_end, ts_start));
	}

	return 0;
}

/**********************************************************/
