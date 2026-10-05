
#include "list.h"
#include <stdarg.h>

/* -------------------------------------------------------------------------- */
/*  double linked list                                                        */
/* -------------------------------------------------------------------------- */
void mimi_list_push_back(mimi_list *list, mimi_node *node)
{
    mimi_assert(list != NULL);
    mimi_assert(node != NULL);

    if (list->head == NULL) {
        list->head = node;
        node->next = node;
        node->prev = node;
        return;
    }

    mimi_node *head = list->head;
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;

    return;
}

void mimi_list_push_front(mimi_list *list, mimi_node *node)
{
    mimi_assert(list != NULL);
    mimi_assert(node != NULL);

    mimi_list_push_back(list, node);
    list->head = node;

    return;
}

void mimi_list_insert_back(mimi_list *list, mimi_node *node, mimi_node *ins)
{
    mimi_assert(list != NULL);
    mimi_assert(node != NULL);
    mimi_assert(ins != NULL);

    ins->prev = node;
    ins->next = node->next;

    node->next->prev = ins;
    node->next = ins;

    return;
}

void mimi_list_insert_front(mimi_list *list, mimi_node *node, mimi_node *ins)
{
    mimi_assert(list != NULL);
    mimi_assert(node != NULL);
    mimi_assert(ins != NULL);

    if (list->head == node) {
        list->head = ins;
    }

    ins->prev = node->prev;
    ins->next = node;

    node->prev->next = ins;
    node->prev = ins;

    return;
}

mimi_node *mimi_list_pop_front(mimi_list *list)
{
    mimi_assert(list != NULL);
    mimi_assert(list->head != NULL);

    mimi_node *node = list->head;
    if (node->next == node) {
        list->head = NULL;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
        list->head = node->next;
    }
    mimi_node_reset(node);

    return node;
}

mimi_node *mimi_list_pop_back(mimi_list *list)
{
    mimi_assert(list != NULL);
    mimi_assert(list->head != NULL);

    mimi_node *tail = list->head->prev;
    if (tail == list->head) {
        list->head = NULL;
    } else {
        tail->prev->next = tail->next;
        tail->next->prev = tail->prev;
    }
    mimi_node_reset(tail);

    return tail;
}

void mimi_list_remove(mimi_list *list, mimi_node *node)
{
    mimi_assert(list != NULL);
    mimi_assert(list->head != NULL);
    mimi_assert(node != NULL);

    if (list->head == node) {
        mimi_list_rotate(list);
    }

    if (list->head == node) {
        list->head = NULL;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    mimi_node_reset(node);
    return;
}
