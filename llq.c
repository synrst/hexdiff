/*
 * llq - linked list queue
 *
 * Provides a doubly linked list queue with functions for appending a new node
 * at the end of the queue, and removing a node from the beginning of the
 * queue. No other insert or remove functions are provided.
 *
 * It is the caller's responsibility to allocate and free all structures in
 * memory, including the object included in each node. To allocate a new list
 * or node, simply use malloc():
 *
 * list = (struct llq_list*)malloc(sizeof(struct llq_list));
 * node = (struct llq_list*)malloc(sizeof(struct llq_list));
 *
 * The llq_list_init() and llq_node_init() functions MUST be used prior to
 * using newly allocated structures to ensure the pointers are properly
 * initialized.
 */

#include <stdio.h>		// NULL
#include "llq.h"

/**********************************************************/
/*
 * Initializes a list that has already been allocated by setting the default
 * values in the structure. This function MUST be called before using a new
 * list. Returns 0 if successful, or -1 if error.
 */
int llq_list_init(llq_list* list) {

	// check parameters
	if (list == NULL) {
		return -1;
	}

	// set default values
	list->size = 0;
	list->head = NULL;
	list->tail = NULL;

	return 0;
}

/**********************************************************/
/*
 * Initializes a node that has already been allocated by setting the default
 * values in the structure, along with setting the given object. This function
 * MUST be called before using a new node. Returns 0 if successful, or -1 if
 * error. Note that the object can be NULL if desired.
 */
int llq_node_init(llq_node* node, void* obj) {

	// check parameters
	if (node == NULL) {
		return -1;
	}

	// set default values
	node->obj = obj;
	node->prev = NULL;
	node->next = NULL;

	return 0;
}

/**********************************************************/
/*
 * Appends a node to the list at the end of the queue and updates the list
 * pointers. Returns 0 if successful, or -1 if error. It is the caller's
 * responsibility to allocate the memory used for the provided node.
 */
int llq_append(llq_list* list, llq_node* node) {

	// check parameters
	if (list == NULL || node == NULL) {
		return -1;
	}

	// do not utilize an uninitialized node
	// prevent appending node to list multiple times
	if (node->prev != NULL || node->next != NULL) {
		return -1;
	}

	// append to empty list
	if (list->head == NULL) {
		list->head = node;
		node->prev = NULL;
		node->next = NULL;
	}

	// append to non-empty list
	else {
		node->prev = list->tail;
		node->next = NULL;
		list->tail->next = node;
	}

	// update tail
	list->tail = node;

	// update size of list
	list->size++;

	return 0;
}

/**********************************************************/
/*
 * Removes a node from the list at the beginning of the queue and updates the
 * list pointers. Returns the removed node if successful, or NULL if error. It
 * is the caller's responsibility to free the memory used by the returned node.
 */
llq_node* llq_remove(llq_list* list) {
	llq_node* node;

	// check parameters
	if (list == NULL) {
		return NULL;
	}

	// empty list
	if (list->head == NULL) {
		return NULL;
	}

	// remove head
	node = list->head;

	// next node is now head
	list->head = node->next;
	if (list->head == NULL) {
		list->tail = NULL;
	}
	else {
		list->head->prev = NULL;
	}

	// detach node from the list
	node->prev = NULL;
	node->next = NULL;

	// update size of list
	list->size--;

	// return pointer to removed node
	return node;
}

/**********************************************************/
