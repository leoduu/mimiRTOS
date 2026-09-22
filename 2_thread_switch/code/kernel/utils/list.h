#ifndef __MIMI_LIST__
#define __MIMI_LIST__

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include "mimi.h"

struct list_node {
    struct list_node *prev;
    struct list_node *next;
};
typedef struct list_node mimi_node;

typedef struct {
    mimi_node *head;
} mimi_list;

mimi_err mimi_list_push_back(mimi_list *list, mimi_node *node);
mimi_err mimi_list_push_front(mimi_list *list, mimi_node *node);
mimi_err mimi_list_insert_back(mimi_list *list, mimi_node *node, mimi_node *ins);
mimi_err mimi_list_insert_front(mimi_list *list, mimi_node *node, mimi_node *ins);
mimi_err mimi_list_pop_front(mimi_list *list);
mimi_err mimi_list_pop_back(mimi_list *list);
mimi_err mimi_list_remove(mimi_list *list, mimi_node *node);
mimi_err mimi_list_rotate(mimi_list *list);
static inline void mimi_list_init(mimi_list *list) {
    list->head = NULL;
}
static inline mimi_bool mimi_list_empty(mimi_list *list) {
    return list->head == NULL;
}
static inline mimi_node *mimi_list_head(mimi_list *list) {
    return list->head;
}
static inline void mimi_node_reset(mimi_node *node) {
    node->prev = NULL;
    node->next = NULL;
}
static inline mimi_bool mimi_node_isolated(mimi_node *node) {
    return node->prev == NULL && node->next == NULL;
}

#define mimi_list_for_each_start(list, node)                    \
    do {                                                        \
        static_assert(__same_type((list), mimi_list *),         \
            "mimi_list_for_each type mismatch");                \
        mimi_node *__head = (list)->head;                       \
        if (__head == NULL) break;                              \
        (node) = __head;                                        \
        do

#define mimi_list_for_each_end(list, node)                      \
        while (((node) = (node)->next) != __head);              \
    } while (0)


#define mimi_list_for_each_safe_start(list, node, tmp)          \
    do {                                                        \
        static_assert(__same_type((list), mimi_list *),         \
            "mimi_list_for_each_safe type mismatch");           \
        mimi_node *__head = (list)->head;                       \
        (node) = __head;                                        \
        (tmp) = (node) ? (node)->next : NULL;                   \
        if ((node) == NULL) break;                              \
        do

#define mimi_list_for_each_safe_end(list, node, tmp)                        \
        while ((node) = (tmp), (tmp) = (node) ? (node)->next : NULL,        \
           (list)->head != NULL && (node) != NULL && (node) != __head);     \
    } while (0)

#endif  // __MIMI_LIST__
