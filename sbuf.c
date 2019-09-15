/*
 * sbuf - structured buffer
 *
 * Provides a structured buffer with the capability to track NULL bytes both
 * before and after the buffer. Also provides a file structure with the
 * capability to read data from a file into a structured buffer.
 */

#include <stdio.h>              // NULL
#include <stdlib.h>		// malloc(), free()
#include <string.h>		// memmove()
#include <unistd.h>		// read(), close(), lseek(), stat()
#include <fcntl.h>		// open()
#include <sys/stat.h>		// open(), stat()
#include <sys/types.h>		// open(), lseek(), stat()
#include "sbuf.h"

/**********************************************************/
/*
 * Allocates memory and initializes a new structured buffer. Returns the new
 * structure, or NULL if error.
 */
sbuf* sbuf_malloc(size_t buf_size) {
	sbuf* sb;

	// check parameters
	if (buf_size == 0) {
		return NULL;
	}

	// allocate memory for structure
	sb = (sbuf*)malloc(sizeof(sbuf));
	if (sb == NULL) {
		return NULL;
	}

	// allocate memory for buffer
	sb->ptr = (unsigned char*)malloc(sizeof(unsigned char) * buf_size);
	if (sb->ptr == NULL) {
		free(sb);
		return NULL;
	}

	// set default values
	sb->size = buf_size;
	sb->pos = 0;
//	sb->before = 0;
	sb->len = 0;

	return sb;
}

/**********************************************************/
/*
 * Frees the memory used by the given structure and the underlying buffer.
 */
void sbuf_free(sbuf* sb) {

	if (sb != NULL) {
		// free buffer
		if (sb->ptr != NULL) {
			free(sb->ptr);
		}

		// free structure
		free(sb);
	}
}

/**********************************************************/
/*
 * Returns the total number of bytes available in the given buffer at the given
 * position, including any NULL bytes before.
 */
size_t sbuf_avail(sbuf* sb, size_t pos) {
	size_t avail;

	// check parametesr
	if (sb == NULL) {
		return 0;
	}

	// at or after end of buffer
	else if (((pos > sb->pos) && ((pos - sb->pos) >= sb->len))) {
		avail = 0;
	}

	// before start of buffer
	else if (pos < sb->pos) {
		avail = (sb->pos - pos) + sb->len;
	}

	// same as buffer
	else if (pos == sb->pos) {
		avail = sb->len;
	}

	// after start of buffer
	else if (pos > sb->pos) {
		avail = sb->len - (pos - sb->pos);
	}

	return avail;
}

/**********************************************************/
/*
 * Returns the number of NULL bytes before the given buffer at the given
 * position.
 */
size_t sbuf_before(sbuf* sb, size_t pos) {
	size_t before;

	// check parameters
	if (sb == NULL) {
		before = 0;
	}

	// at or after end of buffer
	else if (((pos > sb->pos) && ((pos - sb->pos) >= sb->len))) {
		before = 0;
	}

	// before start of buffer
	else if (pos < sb->pos) {
		before = (sb->pos - pos);
	}

	// same as buffer
	else if (pos == sb->pos) {
		before = 0;
	}

	// after start of buffer
	else if (pos > sb->pos) {
		before = 0;
	}

	return before;
}

/**********************************************************/
/*
 * Returns the relative psoition of the given buffer at the given position. If
 * the buffer is preceeded by NULL bytes, the position of the buffer where data
 * resides is returned. Otherwise the given position is returned.
 */
size_t sbuf_rpos(sbuf* sb, size_t pos) {
	size_t rpos;

	// check parameters
	if (sb == NULL) {
		rpos = pos;
	}

	// at or after end of buffer
	else if (((pos > sb->pos) && ((pos - sb->pos) >= sb->len))) {
		rpos = pos;
	}

	// before start of buffer
	else if (pos < sb->pos) {
		rpos = sb->pos;
	}

	// same as buffer
	else if (pos == sb->pos) {
		rpos = pos;
	}

	// after start of buffer
	else if (pos > sb->pos) {
		rpos = pos;
	}

	return rpos;
}

/**********************************************************/
/*
 * Returns a pointer into the given buffer relative to the given position.
 * Returns NULL if data is not available at the given position because the
 * buffer has already reached the end. If the given position is before the
 * position of the buffer, the start of the buffer is returned. It may be
 * necessary to use sbuf_before() to check how many NULL bytes preceed the
 * buffer, or sbuf_rpos() to obtain the relative position of the returned
 * buffer pointer.
 */
unsigned char* sbuf_ptr(sbuf* sb, size_t pos) {
	unsigned char* ptr;

	if (sb == NULL) {
		ptr = NULL;
	}

	// at or after end of buffer
	else if (((pos > sb->pos) && ((pos - sb->pos) >= sb->len))) {
		ptr = NULL;
	}

	// before start of buffer
	// NOTE: the before length MUST be checked for this to be accurate
	else if (pos < sb->pos) {
		ptr = sb->ptr;
	}

	// same as buffer
	else if (pos == sb->pos) {
		ptr = sb->ptr;
	}

	// after start of buffer
	else if (pos > sb->pos) {
		ptr = sb->ptr + (pos - sb->pos);
	}

	return ptr;
}

/**********************************************************/
/*
 * Returns a pointer into the given buffer relative to the given position for
 * extracting a single character. Returns NULL if a character is not available
 * at the given position. Unlike sbuf_ptr(), always returns NULL if the given
 * position is outside of the buffer.
 */
unsigned char* sbuf_char(sbuf* sb, size_t pos) {

	// check parameters
	if (sb == NULL) {
		return NULL;
	}

	// before
	if (pos < sb->pos) {
		return NULL;
	}

	// after
	if (pos >= (sb->pos + sb->len)) {
		return NULL;
	}

	// within buffer
	return (sb->ptr + (pos - sb->pos));
}

/**********************************************************/
/*
 * Returns the value of the data at the given position using the full bytes of
 * the given word size. As an example, if the word size is four, a four byte
 * value is returned. If the word size is one, only a one byte value is
 * returned. Any NULL bytes in the word are assumed to be zero.
 */
size_t sbuf_word(sbuf* sb, size_t pos, size_t word_size) {
	size_t i;
	size_t value;

	// check parameters
	if (sb == NULL) {
		return 0;
	}

	// before
	if (pos < sb->pos) {
		return 0;
	}

	// after
	if (pos >= (sb->pos + sb->len)) {
		return 0;
	}

	// within buffer
	value = 0;
	i = pos;
	while ((i >= sb->pos) && (i < (sb->pos + sb->len)) && (i < (pos + word_size))) {

		// include character in value
		// shift existing value
		value = (value << 8) | *(unsigned char*)(sb->ptr + (i - sb->pos));

		// increment position
		i++;
	}

	// continue shifting to pad for NULL bytes
	while (i < (pos + word_size)) {
		value <<= 8;
		i++;
	}

	return value;
}

/**********************************************************/
/*
 * Returns the length of the data starting at the pointer returned from
 * calling sbuf_ptr().
 */
size_t sbuf_len(sbuf* sb, size_t pos) {
	size_t len;

	// check parameters
	if (sb == NULL) {
		len = 0;
	}

	// at or after end of buffer
	if (pos >= (sb->pos + sb->len)) {
		len = 0;
	}

	// before start of buffer
	else if (pos < sb->pos) {
		len = sb->len;
	}

	// same as buffer
	else if (pos == sb->pos) {
		len = sb->len;
	}

	// after start of buffer
	else if (pos > sb->pos) {
		len = sb->len - (pos - sb->pos);
	}

	return len;
}

/**********************************************************/
/*
 * Reduces the data in the given buffer by removing all data up to the given
 * position and moves data at the given position and after to the beginning
 * of the buffer. This result in extra space at the end of the buffer to read
 * in more data. Returns 0 if successful, or -1 if error.
 */
int sbuf_reduce(sbuf* sb, size_t pos) {
	size_t rbytes;

	// check parameters
	if (sb == NULL) {
		return -1;
	}

	// position is at or before the buffer, do nothing
	if (pos <= sb->pos) {
	}

	// position is after start of buffer
	else {
		// determine the amount of data that can be removed
		rbytes = pos - sb->pos;

		// buffer is bigger, shift data in buffer
		if (sb->len > rbytes) {
			memmove(sb->ptr, sb->ptr + rbytes, sb->len - rbytes);
			sb->pos += rbytes;
			sb->len -= rbytes;
		}

		// buffer is same size or smaller, empty entire buffer
		// but set position to current position plus current length
		else if (sb->len <= rbytes) {
			sb->pos += sb->len;
			sb->len = 0;
		}
	}

	return 0;
}

/**********************************************************/
/*
 * Allocates memory and initializes a new file structure. Returns the new
 * structure, or NULL if error.
 */
sfile* sfile_malloc(void) {
	sfile* sf;

	// allocate memory for structure
	sf = (sfile*)malloc(sizeof(sfile));
	if (sf == NULL) {
		return NULL;
	}

	// initialize file structure
	sf->fd = -1;
	sf->eof = 0;
	sf->start_pos = 0;
	sf->bytes_read = 0;

	return sf;
}

/**********************************************************/
/*
 * Frees the memory used by the given file structure.
 */
void sfile_free(sfile* sf) {

	if (sf != NULL) {
		free(sf);
	}
}

/**********************************************************/
/*
 * Opens the file at the given path and stores the file descriptor in the given
 * structure. Returns 0 if successful, or < 0 if error.
 */
int sfile_open(sfile* sf, char* path) {
	struct stat buf;

	// check parameters
	if (sf == NULL || path == NULL) {
		return -1;
	}

	// file is already open
	if (sf->fd > 0) {
		return -1;
	}


	// STDIN
	if (strcmp(path, "-") == 0) {
		sf->fd = fileno(stdin);
	}
	// file
	else {

		// ensure file is not a directory
		if (stat(path, &buf) < 0) {
			return -1;
		}
		if ((buf.st_mode & S_IFDIR) == S_IFDIR) {
			return -1;
		}

		// open file
		sf->fd = open(path, O_RDONLY, 0666);
	}

	// invalid file
	if (sf->fd < 0) {
		return sf->fd;
	}

	// reset values
	sf->eof = 0;
	sf->bytes_read = 0;

	return 0;
}

/**********************************************************/
/*
 * Closes the given file. Return 0 if successful, or < 0 on error.
 */
int sfile_close(sfile* sf) {

	// check parameters
	if (sf == NULL) {
		return -1;
	}

	if (sf->fd < 0) {
		return -1;
	}

	return close(sf->fd);
}

/**********************************************************/
/*
 * Reads data from the given file and appends it to the given buffer. Attempts
 * to read enough data to fill the entire buffer, but can be limited by how
 * much data is actually returned by a single read. Returns the number of bytes
 * read, or 0 if eof, or < 0 if error.
 */
ssize_t sfile_read(sfile* sf, sbuf* sb) {
	size_t read_size;
	ssize_t br;

	// check parameters
	if (sb == NULL) {
		return 0;
	}

	// enf-of-file
	if (sf->eof) {
		return 0;
	}

	// determine the number of bytes to read
	// size of buffer minus current length of data
	read_size = sb->size - sb->len;

	// buffer is already full
	if (read_size == 0) {
		return -1;
	}

	// read from file
	br = read(sf->fd, sb->ptr + sb->len, read_size);
	if (br > 0) {
		sb->len += br;
		sf->bytes_read += br;
	}
	else if (br == 0) {
		sf->eof = 1;
	}
	else {
		// error, do nothing
	}

	return br;
}

/**********************************************************/
/*
 * Seeks the given file to the specified position. This function should only be
 * called once immediately after calling sbuf_open(), otherwise the relative
 * position may be incorrectly set. If lseek() cannot be used on the given file
 * (such as STDIN), the position is reached by reading the appropriate amount
 * of data. In this case, the structured buffer is used to hold the data as it
 * is read from the file.
 */
int sfile_seek(sfile* sf, sbuf* sb, size_t pos) {
	off_t off;
	ssize_t br;

	// check parameters
	if (sf == NULL || sb == NULL) {
		return -1;
	}
	if (pos == 0) {
		return 0;
	}

	// inconsistencies with types
	if ((size_t)(off_t)pos != pos) {
		fprintf(stderr, "sfile_init_seek(pos=%zu) truncated\n", pos);
		exit(1);
	}

	// seek to position
	off = lseek(sf->fd, pos, SEEK_SET);

	// lseek() doesn't work on STDIN
	if (off < 0) {

		// loop reading file
		while (sb->pos < pos) {

			// reduce current buffer to zero
			sbuf_reduce(sb, sb->pos + sb->len);

			// read more data into buffer
			br = sfile_read(sf, sb);

			// position is past EOF
			if (br <= 0) {
				sbuf_reduce(sb, pos);

				// EOF, break loop
				break;
			}

			// position is within buffer
			else if (sb->pos < pos && pos <= sb->pos + sb->len) {
				sbuf_reduce(sb, pos);
			}
		}

		// adjust bytes read as if seeking was performed
		if (sf->bytes_read > pos) {
			sf->bytes_read -= pos;
		}
		else {
			sf->bytes_read = 0;
		}
	}

	return 0;
}

/**********************************************************/
/*
 * Shifts the position in the given file and structured buffer by the given
 * length, adding NULL bytes to the beginning of the buffer.
 */
int sfile_shift(sfile* sf, sbuf* sb, size_t len) {

	// check parameters
	if (sb == NULL) {
		return -1;
	}

	// shift current position of file and buffer
	sb->pos += len;
	sf->start_pos += len;

	return 0;
}

/**********************************************************/
/*
 * Returns 1 if the given buffer is considered end-of-output at the given
 * position. The given file associated with this buffer must be end-of-file
 * for the buffer to be considered end-of-output, and the position must be at
 * or after the end of the buffer.
 */
int sfile_eoo(sfile* sf, sbuf* sb, size_t pos) {

	// check parameters
	if (sb == NULL) {
		return -1;
	}
	if (sf == NULL) {
		return -1;
	}

	// end-of-file and at or after end of buffer
	if (sf->eof && (pos >= sb->pos) && ((pos - sb->pos) >= sb->len)) {
		return 1;
	}

	return 0;
}

/**********************************************************/
