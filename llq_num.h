#ifndef _LLQ_NUM_H
#define _LLQ_NUM_H

// llq num functions
llq_list* llq_num_malloc(void);
int llq_num_free(llq_list* list);
int llq_num_append(llq_list* list, size_t value);
size_t llq_num_value(llq_node* node);

#endif /* _LLQ_NUM_H */
