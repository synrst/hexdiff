#ifndef _LLQ_H
#define _LLQ_H

// node
struct llq_node {
	void* obj;			// generic pointer to object
	struct llq_node* prev;		// previous node in list
	struct llq_node* next;		// next node in list
};
typedef struct llq_node llq_node;

// list
struct llq_list {
	size_t size;			// number of nodes in list
	struct llq_node* head;		// head of list
	struct llq_node* tail;		// tail of list
};
typedef struct llq_list llq_list;

// llq functions
int llq_list_init(llq_list* list);
int llq_node_init(llq_node* node, void* obj);
int llq_append(llq_list* list, llq_node* node);
llq_node* llq_remove(llq_list* list);

#endif /* _LLQ_H */
