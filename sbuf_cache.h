#ifndef _SBUF_CACHE_H
#define _SBUF_CACHE_H

#include "llq.h"
#include "sbuf.h"

// cache node functions
llq_node* sbuf_cache_node_malloc(size_t len);
int sbuf_cache_node_free(llq_node*);

// cache
struct sbuf_cache {
        llq_list active;		// active cache list
        llq_list inactive;		// inactive cache list
        size_t max_buf_len;		// fixed length for each buffer
};
typedef struct sbuf_cache sbuf_cache;

// cache functions
sbuf_cache* sbuf_cache_malloc(size_t max_buf_len);
int sbuf_cache_free(sbuf_cache* cache);

llq_node* sbuf_cache_node_malloc(size_t size);
int sbuf_cache_node_free(llq_node* node);

llq_node* sbuf_cache_append(sbuf_cache* cache, unsigned char* ptr, size_t pos, size_t len);
sbuf* sbuf_cache_remove(sbuf_cache* cache);
int sbuf_cache_purge(sbuf_cache* cache);
int sbuf_cache_reduce(sbuf_cache* cache);

#endif /* _SBUF_CACHE_H */
