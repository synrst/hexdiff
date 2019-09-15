#ifndef _SBUF_H
#define _SBUF_H

struct sbuf {
	unsigned char* ptr;	// pointer to buffer
	size_t size;		// maximum size of buffer (should not change)
	size_t pos;		// position of buffer
//	size_t before;		// number of null bytes before data
	size_t len;		// length of actual data in buffer
};
typedef struct sbuf sbuf;

struct sfile {
	int fd;
	int eof;		// flag to mark end-of-file
	size_t start_pos;	// starting position (for calculating length)
	size_t bytes_read;	// total bytes read
};
typedef struct sfile sfile;

sbuf* sbuf_malloc(size_t buf_size);
void sbuf_free(sbuf* sb);

size_t sbuf_avail(sbuf* sb, size_t pos);
size_t sbuf_before(sbuf* sb, size_t pos);
size_t sbuf_rpos(sbuf* sb, size_t pos);
unsigned char* sbuf_ptr(sbuf* sb, size_t pos);
size_t sbuf_word(sbuf* sb, size_t pos, size_t word_size);
unsigned char* sbuf_char(sbuf* sb, size_t pos);
size_t sbuf_len(sbuf* sb, size_t pos);
int sbuf_reduce(sbuf* sb, size_t pos);

sfile* sfile_malloc(void);
void sfile_free(sfile* sf);

int sfile_open(sfile* sf, char* name);
int sfile_close(sfile* sf);
ssize_t sfile_read(sfile* sf, sbuf* sb);
int sfile_seek(sfile* sf, sbuf* sb, size_t pos);
int sfile_shift(sfile* sf, sbuf* sb, size_t len);
int sfile_eoo(sfile* sf, sbuf* sb, size_t pos);

#endif /* _SBUF_H */
