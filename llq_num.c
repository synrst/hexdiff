/*
 * llq_num - linked list queue for a number list
 *
 * Provides a doubly linked list queue with functions for storing a list of
 * unsigned numbers of type size_t.
 */
#include <stdio.h>		// NULL
#include <stdlib.h>		// malloc(), free()
#include "llq.h"
#include "llq_num.h"

/**********************************************************/
/*
 * Compile time assertion to ensure types are the same size. If not, a
 * separate size_t object will need to be allocated in llq_num_append(), and a
 * pointer to that object will need to be stored in the node. This object will
 * also need to be freed when the list is destroyed in llq_num_free().
 */
#include <assert.h>
#ifdef static_assert
static_assert(sizeof(size_t)==sizeof(void*),"sizeof(size_t) != sizeof(void*)");
#endif

/**********************************************************/
/*
 * Allocates and initializes a new empty linked list in memory. Returns a
 * pointer to the new linked list, or NULL if error.
 */
llq_list* llq_num_malloc(void) {
	llq_list* list;

	// allocate memory for linked list
	list = (llq_list*)malloc(sizeof(llq_list));
	if (list == NULL) {
		return NULL;
	}

	// initialize
	if (llq_list_init(list) < 0) {
		free(list);
		return NULL;
	}

	return list;
}

/**********************************************************/
/*
 * Frees the memory used by all of the nodes in the given linked listed and the
 * memory used by the linked list itself. Return 0 if successful, or -1 if
 * error.
 */
int llq_num_free(llq_list* list) {
	llq_node* node;

	// check parameters
	if (list == NULL) {
		return -1;
	}

	// remove nodes until there are no more
	node = llq_remove(list);
	while (node != NULL) {

		// free node
		free(node);

		// get next node
		node = llq_remove(list);
	}

	// free linked list
	free(list);

	return 0;
}

/**********************************************************/
/*
 * Appends the given value to the linked list. Allocates a new node and adds it
 * to the list. Returns 0 if successful, or -1 if error.
 */
int llq_num_append(llq_list* list, size_t value) {
	llq_node* node;

	// check parameters
	if (list == NULL) {
		return -1;
	}

	// allocate new node
	node = (llq_node*)malloc(sizeof(llq_node));
	if (node == NULL) {
		return -1;
	}

	// initialize node with value
	if (llq_node_init(node, (void*)value) < 0) {
		free(node);
		return -1;
	}

	// add node to list
	if (llq_append(list, node) < 0) {
		free(node);
		return -1;
	}

	return 0;
}

/**********************************************************/
/*
 * Returns the value associated with the given node. Returns 0 if node is NULL.
 */
size_t llq_num_value(llq_node* node) {

	// check pamareters
	if (node == NULL) {
		return 0;
	}

	return (size_t)node->obj;
}

/**********************************************************/
/*
 * Sample loop.
 */

/*
llq_node* node;
size_t value;

// get first node
node = list->head;

while (node != NULL) {

	// get value from node
	value = llq_num_value(node);

	// ... do something with value

	// get next node
	node = node->next;
}
 */
/**********************************************************/
