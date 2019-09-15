/*
 * sbuf_cache - linked list queue with buffered cache
 *
 * Provides a doubly linked list queue with functions for caching fixed length
 * data buffers. The cache will automatically allocate new nodes as needed, and
 * reuse existing nodes that are no longer active.
 */

#include <stdio.h>		// NULL
#include <stdlib.h>		// malloc(), free()
#include <string.h>		// memcpy()
#include "llq.h"
#include "sbuf.h"
#include "sbuf_cache.h"

/**********************************************************/
/*
 * Allocates and initializes a new empty cache in memory and stores the maximum
 * buffer length of each node. Returns the new structure, or NULL if error.
 */
sbuf_cache* sbuf_cache_malloc(size_t max_buf_len) {
	sbuf_cache* cache;

	// allocate memory for cache structrue
	cache = (sbuf_cache*)malloc(sizeof(sbuf_cache));
	if (cache == NULL) {
		return NULL;
	}

	// set the maximum buffer length available to each node
	cache->max_buf_len = max_buf_len;

	return cache;
}

/**********************************************************/
/*
 * Frees the memory used by the given cache, and frees the memory used by any
 * attached nodes. Returns 0 if successful, or -1 if error.
 */
int sbuf_cache_free(sbuf_cache* cache) {

	// check parameters
	if (cache == NULL) {
		return -1;
	}

	// move all actives nodes to inactive
	sbuf_cache_purge(cache);

	// free all inactive nodes
	sbuf_cache_reduce(cache);

	// free cache structure
	free(cache);

	return 0;
}

/**********************************************************/
/*
 * Allocates a new cache node in memory and allocates the corresponding data
 * buffer of the given length, and sets the default values. The node will
 * initially have 0 bytes of data even though the buffer can hold up to the
 * given length. Returns the new structure, or NULL if error.
 */
llq_node* sbuf_cache_node_malloc(size_t size) {
	llq_node* node;
	sbuf* obj;

	// allocate node
	node = (llq_node*)malloc(sizeof(llq_node));
	if (node == NULL) {
		return NULL;
	}

	// allocate cache buffer object
	obj = (sbuf*)malloc(sizeof(sbuf));
	if (obj == NULL) {
		free(node);
		return NULL;
	}

	// allocate actual buffer
	obj->ptr = (unsigned char*)malloc(sizeof(unsigned char) * size);
	if (obj->ptr == NULL) {
		free(obj);
		free(node);
		return NULL;
	}

	// set default values
	obj->size = size;
	obj->pos = 0;
	obj->len = 0;

	// initialize
	llq_node_init(node, obj);

	return node;
}

/**********************************************************/
/*
 * Frees the memory used by the given cache node, and frees the the memory used
 * by the corresponding data buffer. A node must be detached from a list before
 * the memory can be freed. Returns 0 if successful, or -1 if error.
 */
int sbuf_cache_node_free(llq_node* node) {
	sbuf* obj;

	// check parameters
	if (node == NULL) {
		return -1;
	}

	// ensure the node is detached from a list
	if (node->prev != NULL || node->next != NULL) {
		return -1;
	}

	// cast obj to cache buffer
	obj = (sbuf*)node->obj;

	if (obj != NULL) {

		// free buffer
		if (obj->ptr != NULL) {
			free(obj->ptr);
		}

		// free object
		free(obj);
	}

	// free node
	free(node);

	return 0;
}

/**********************************************************/
/*
 * Copies the given buffer to the cache queue. If the inactive list is not
 * empty, it will remove a node from the inactive list and reuse it. Otherwise
 * it will allocate a new node using the maximum buffer length assigned to the
 * cache, increasing the overall size of the cache. Returns a pointer to the
 * node that was appended to the active list, or NULL if error. If the given
 * buffer is larger than the size of the buffer in the node, the copied buffer
 * will be truncated.
 *
 * This function assumes that EVERY node in the cache has a buffer of the same
 * length (the maximum buffer length), but this can be violated if nodes are
 * manually appended to the cache outside of this function. It is recommended
 * to always use this function to add new data to the cache.
 */
llq_node* sbuf_cache_append(sbuf_cache* cache, unsigned char* buf, size_t pos, size_t len) {
	llq_node* node;
	sbuf* obj;

	// check parameters
	if (cache == NULL) {
		return NULL;
	}

	// truncate length for NULL buffer
	if (buf == NULL) {
		len = 0;
	}

	// truncate to maximum length
	if (len > cache->max_buf_len) {
		len = cache->max_buf_len;
	}

	// attempt to reuse an inactive node
	node = llq_remove(&cache->inactive);

	// create new node
	if (node == NULL) {
		node = sbuf_cache_node_malloc(cache->max_buf_len);
		if (node == NULL) {
			return NULL;
		}
	}

	// use node object as buffer
	obj = (sbuf*)node->obj;

	// truncate to buffer length of node
	if (len > obj->size) {
		len = obj->size;
	}

	// copy position to cache node
	obj->pos = pos;

	// copy length to cache node
	obj->len = len;

	// copy buffer to cache node
	if (obj->len > 0) {
		memcpy(obj->ptr, buf, obj->len);
	}

	// append node to active cache
	if (llq_append(&cache->active, node) != 0) {

		// node is not in a list, free memory
		sbuf_cache_node_free(node);
		return NULL;
	}

	return node;
}

/**********************************************************/
/*
 * Removes and returns the next buffer from the cache queue. Removes the node
 * from the active list and appends it to the inactive list. Returns a pointer
 * to the cache buffer structure of the node, or NULL if error. Also returns
 * NULL if the active list of the cache is empty.
 *
 * The caller shall NOT free or modify the returned cache buffer. The node
 * containing the cache buffer is moved to the inactive list and may be reused
 * by sbuf_cache_append(). It is the caller's responsibility to immediately use
 * the cache buffer or copy the data from the cache buffer.
 */
sbuf* sbuf_cache_remove(sbuf_cache* cache) {
	llq_node* node;

	// check parameters
	if (cache == NULL) {
		return NULL;
	}

	// remove node from active cache
	node = llq_remove(&cache->active);

	// cache is empty
	if (node == NULL) {
		return NULL;
	}

	// move node to inactive cache
	llq_append(&cache->inactive, node);

	// return cache buffer
	return (sbuf*)node->obj;
}

/**********************************************************/
/*
 * Purges the cache by moving all nodes from the active cache list to the
 * inactive cache list. The data residing in the cache buffers should be
 * considered no longer available. Returns 0 if successful, of -1 if error.
 */
int sbuf_cache_purge(sbuf_cache* cache) {
	llq_node* node;

	// check parameters
	if (cache == NULL) {
		return -1;
	}

	// loop through and remove active nodes
	node = llq_remove(&cache->active);
	while (node != NULL) {

		// append node to inactive list
		llq_append(&cache->inactive, node);

		// remove next node
		node = llq_remove(&cache->active);
	}

	return 0;
}

/**********************************************************/
/*
 * Reduces the amount of memory in use by the cache by freeing all of the nodes
 * in the inactive cache list. If the inactive list is empty, no additional
 * memory will be freed. Returns 0 if successful, or -1 if error.
 */
int sbuf_cache_reduce(sbuf_cache* cache) {
	llq_node* node;

	// check parameters
	if (cache == NULL) {
		return -1;
	}

	// loop through inactive nodes
	node = llq_remove(&cache->inactive);
	while (node != NULL) {

		// free node
		sbuf_cache_node_free(node);

		// get next node
		node = llq_remove(&cache->inactive);
	}
	return 0;
}

/**********************************************************/
