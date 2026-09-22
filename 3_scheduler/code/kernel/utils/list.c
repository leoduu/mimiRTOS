
#include "list.h"
#include <stdarg.h>

/* -------------------------------------------------------------------------- */
/*  double linked list                                                        */
/* -------------------------------------------------------------------------- */
mimi_err mimi_list_push_back(mimi_list *list, mimi_node *node)
{
    mimi_assert(list != NULL);
    mimi_assert(node != NULL);

    if (list->head == NULL) {
        list->head = node;
        node->next = node;
        node->prev = node;
        return MIMI_EOK;
    }

    mimi_node *head = list->head;
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;

    return MIMI_EOK;
}

mimi_err mimi_list_push_front(mimi_list *list, mimi_node *node)
{
    mimi_assert(list != NULL);
    mimi_assert(node != NULL);

    if (mimi_list_push_back(list, node) != MIMI_EOK) {
        return MIMI_EPARAMETER;
    }
    list->head = node;

    return MIMI_EOK;
}

mimi_err mimi_list_insert_back(mimi_list *list, mimi_node *node, mimi_node *ins)
{
    mimi_assert(list != NULL);
    mimi_assert(node != NULL);
    mimi_assert(ins != NULL);

    ins->prev = node;
    ins->next = node->next;

    node->next->prev = ins;
    node->next = ins;


    return MIMI_EOK;
}

mimi_err mimi_list_insert_front(mimi_list *list, mimi_node *node, mimi_node *ins)
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


    return MIMI_EOK;
}

mimi_err mimi_list_pop_front(mimi_list *list)
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

    return MIMI_EOK;
}

mimi_err mimi_list_pop_back(mimi_list *list)
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

    return MIMI_EOK;
}

mimi_err mimi_list_remove(mimi_list *list, mimi_node *node)
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
    return MIMI_EOK;
}

mimi_err mimi_list_rotate(mimi_list *list)
{
    mimi_assert(list != NULL);
    mimi_assert(list->head != NULL);

    list->head = list->head->next;

    return MIMI_EOK;
}
