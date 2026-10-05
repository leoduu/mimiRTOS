#include <stdlib.h>
#include <unity.h>

#include "list.h"
#include "test_mocks.h"

static mimi_node *make_node(void)
{
    mimi_node *n = (mimi_node *)malloc(sizeof(mimi_node));
    TEST_ASSERT_NOT_NULL(n);
    n->prev = n;
    n->next = n;
    return n;
}

static void test_init(void)
{
    mimi_list list;
    mimi_list_init(&list);
    TEST_ASSERT_NULL(list.head);
    TEST_ASSERT_TRUE(mimi_list_empty(&list));
}

static void test_node_reset_direct(void)
{
    mimi_node n;
    n.prev = &n;
    n.next = &n;

    mimi_node_reset(&n);
    TEST_ASSERT_NULL(n.prev);
    TEST_ASSERT_NULL(n.next);
    TEST_ASSERT_TRUE(mimi_node_isolated(&n));
}

static void test_push_back_one(void)
{
    mimi_list list;
    mimi_node *n1 = make_node();
    mimi_list_init(&list);

    mimi_list_push_back(&list, n1);

    TEST_ASSERT_EQUAL_PTR(n1, list.head);
    TEST_ASSERT_EQUAL_PTR(n1, n1->prev);
    TEST_ASSERT_EQUAL_PTR(n1, n1->next);
    TEST_ASSERT_FALSE(mimi_list_empty(&list));

    free(n1);
}

static void test_push_back_multiple(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_node *n3 = make_node();

    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);
    mimi_list_push_back(&list, n3);

    TEST_ASSERT_EQUAL_PTR(n1, list.head);
    TEST_ASSERT_EQUAL_PTR(n3, list.head->prev);
    TEST_ASSERT_EQUAL_PTR(n2, list.head->next);
    TEST_ASSERT_EQUAL_PTR(n1, n2->prev);
    TEST_ASSERT_EQUAL_PTR(n3, n2->next);
    TEST_ASSERT_EQUAL_PTR(n2, n3->prev);
    TEST_ASSERT_EQUAL_PTR(n1, n3->next);

    free(n1); free(n2); free(n3);
}

static void test_push_front(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_node *n3 = make_node();

    mimi_list_push_front(&list, n1);
    mimi_list_push_front(&list, n2);
    mimi_list_push_front(&list, n3);

    TEST_ASSERT_EQUAL_PTR(n3, list.head);
    TEST_ASSERT_EQUAL_PTR(n2, list.head->next);
    TEST_ASSERT_EQUAL_PTR(n1, list.head->next->next);
    TEST_ASSERT_EQUAL_PTR(n1, list.head->prev);

    free(n1); free(n2); free(n3);
}

static void test_insert_back(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_node *n3 = make_node();

    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);
    mimi_list_insert_back(&list, n1, n3);   /* 在 n1 后面插入 n3 → n1 <-> n3 <-> n2 */

    TEST_ASSERT_EQUAL_PTR(n1, list.head);
    TEST_ASSERT_EQUAL_PTR(n3, n1->next);
    TEST_ASSERT_EQUAL_PTR(n1, n3->prev);
    TEST_ASSERT_EQUAL_PTR(n2, n3->next);
    TEST_ASSERT_EQUAL_PTR(n3, n2->prev);

    free(n1); free(n2); free(n3);
}

static void test_insert_back_after_tail(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_node *n3 = make_node();

    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);
    /* n2 is the tail (list->head->prev) — insert n3 after it */
    mimi_list_insert_back(&list, n2, n3);

    /* head must NOT change — n3 was inserted at tail position */
    TEST_ASSERT_EQUAL_PTR(n1, list.head);
    TEST_ASSERT_EQUAL_PTR(n2, n1->next);
    TEST_ASSERT_EQUAL_PTR(n3, n2->next);
    TEST_ASSERT_EQUAL_PTR(n1, n3->next);  /* circular back to head */
    TEST_ASSERT_EQUAL_PTR(n3, n1->prev);  /* n3 is now tail */

    free(n1); free(n2); free(n3);
}

static void test_insert_front(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_node *n3 = make_node();

    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);
    mimi_list_insert_front(&list, n2, n3);   /* 在 n2 前面插入 n3 → n1 <-> n3 <-> n2 */

    TEST_ASSERT_EQUAL_PTR(n1, list.head);
    TEST_ASSERT_EQUAL_PTR(n3, n1->next);
    TEST_ASSERT_EQUAL_PTR(n1, n3->prev);
    TEST_ASSERT_EQUAL_PTR(n2, n3->next);
    TEST_ASSERT_EQUAL_PTR(n3, n2->prev);

    free(n1); free(n2); free(n3);
}

static void test_pop_front(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);

    TEST_ASSERT_EQUAL_PTR(n1, mimi_list_pop_front(&list));
    TEST_ASSERT_EQUAL_PTR(n2, list.head);
    TEST_ASSERT_NULL(n1->prev);
    TEST_ASSERT_NULL(n1->next);
    TEST_ASSERT_TRUE(mimi_node_isolated(n1));
    TEST_ASSERT_EQUAL_PTR(n2, n2->prev);
    TEST_ASSERT_EQUAL_PTR(n2, n2->next);

    TEST_ASSERT_EQUAL_PTR(n2, mimi_list_pop_front(&list));
    TEST_ASSERT_NULL(list.head);
    TEST_ASSERT_TRUE(mimi_list_empty(&list));
    TEST_ASSERT_TRUE(mimi_node_isolated(n2));

    free(n1); free(n2);
}

static void test_pop_back(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);

    TEST_ASSERT_EQUAL_PTR(n2, mimi_list_pop_back(&list));
    TEST_ASSERT_EQUAL_PTR(n1, list.head);
    TEST_ASSERT_EQUAL_PTR(n1, n1->prev);
    TEST_ASSERT_EQUAL_PTR(n1, n1->next);
    TEST_ASSERT_TRUE(mimi_node_isolated(n2));

    TEST_ASSERT_EQUAL_PTR(n1, mimi_list_pop_back(&list));
    TEST_ASSERT_NULL(list.head);

    free(n1); free(n2);
}

static void test_rotate(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_node *n3 = make_node();
    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);
    mimi_list_push_back(&list, n3);

    TEST_ASSERT_EQUAL_PTR(n1, list.head);
    mimi_list_rotate(&list);
    TEST_ASSERT_EQUAL_PTR(n2, list.head);
    mimi_list_rotate(&list);
    TEST_ASSERT_EQUAL_PTR(n3, list.head);
    mimi_list_rotate(&list);
    TEST_ASSERT_EQUAL_PTR(n1, list.head);

    free(n1); free(n2); free(n3);
}

static void test_rotate_single(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_list_push_back(&list, n1);

    TEST_ASSERT_EQUAL_PTR(n1, list.head);
    mimi_list_rotate(&list);
    TEST_ASSERT_EQUAL_PTR(n1, list.head);

    free(n1);
}

static void test_reuse_after_empty(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);
    mimi_list_pop_front(&list);
    mimi_list_pop_front(&list);
    TEST_ASSERT_NULL(list.head);

    /* 清空后再次 push */
    mimi_node *n3 = make_node();
    mimi_list_push_back(&list, n3);
    TEST_ASSERT_EQUAL_PTR(n3, list.head);
    TEST_ASSERT_EQUAL_PTR(n3, n3->prev);
    TEST_ASSERT_EQUAL_PTR(n3, n3->next);

    mimi_list_push_front(&list, n1);
    TEST_ASSERT_EQUAL_PTR(n1, list.head);
    TEST_ASSERT_EQUAL_PTR(n3, list.head->next);

    free(n1); free(n2); free(n3);
}

static void test_remove(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_node *n3 = make_node();
    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);
    mimi_list_push_back(&list, n3);

    /* 移除中间节点 n2 */
    mimi_list_remove(&list, n2);
    TEST_ASSERT_TRUE(mimi_node_isolated(n2));
    TEST_ASSERT_EQUAL_PTR(n1, list.head);
    TEST_ASSERT_EQUAL_PTR(n3, n1->next);
    TEST_ASSERT_EQUAL_PTR(n1, n3->prev);

    /* 移除头节点 n1 */
    mimi_list_remove(&list, n1);
    TEST_ASSERT_EQUAL_PTR(n3, list.head);
    TEST_ASSERT_EQUAL_PTR(n3, n3->prev);
    TEST_ASSERT_EQUAL_PTR(n3, n3->next);

    /* 移除最后一个节点 n3 */
    mimi_list_remove(&list, n3);
    TEST_ASSERT_NULL(list.head);

    free(n1); free(n2); free(n3);
}

static void test_remove_single(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_list_push_back(&list, n1);

    mimi_list_remove(&list, n1);
    TEST_ASSERT_NULL(list.head);
    TEST_ASSERT_TRUE(mimi_node_isolated(n1));

    free(n1);
}

static void test_for_each_count(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_node *n3 = make_node();
    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);
    mimi_list_push_back(&list, n3);

    int count = 0;
    mimi_node *node;
    mimi_list_for_each_start(&list, node) {
        count++;
    } mimi_list_for_each_end(&list, node);

    TEST_ASSERT_EQUAL_INT(3, count);

    free(n1); free(n2); free(n3);
}

static void test_for_each_single(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_list_push_back(&list, n1);

    int count = 0;
    mimi_node *node;
    mimi_list_for_each_start(&list, node) {
        count++;
    } mimi_list_for_each_end(&list, node);

    TEST_ASSERT_EQUAL_INT(1, count);

    free(n1);
}

static void test_for_each_safe_remove_middle(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_node *n3 = make_node();
    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);
    mimi_list_push_back(&list, n3);

    /* 遍历时移除中间节点 n2，头未变，safe 遍历不受影响 */
    mimi_node *node;
    mimi_node *tmp;
    int count = 0;
    mimi_list_for_each_safe_start(&list, node, tmp) {
        if (node == n2) {
            mimi_list_remove(&list, node);
        }
        count++;
    } mimi_list_for_each_safe_end(&list, node, tmp);

    /* 遍历了所有原始 3 个节点（含被移除的 n2） */
    TEST_ASSERT_EQUAL_INT(3, count);
    /* n2 被移除后 n1 / n3 链接正确 */
    TEST_ASSERT_EQUAL_PTR(n1, list.head);
    TEST_ASSERT_EQUAL_PTR(n3, n1->next);
    TEST_ASSERT_EQUAL_PTR(n1, n3->prev);

    free(n1); free(n2); free(n3);
}

static void test_for_each_safe_empty_after_pop(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_list_push_back(&list, n1);

    /* pop-all during safe traversal: list->head becomes NULL,
     * _end catches it via (list)->head != NULL — single pass, no extra run */
    mimi_node *node;
    mimi_node *tmp;
    int count = 0;
    mimi_list_for_each_safe_start(&list, node, tmp) {
        mimi_list_pop_front(&list);
        count++;
    } mimi_list_for_each_safe_end(&list, node, tmp);

    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_NULL(list.head);

    free(n1);
}

static void test_for_each_safe_pop_all(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n1 = make_node();
    mimi_node *n2 = make_node();
    mimi_node *n3 = make_node();
    mimi_list_push_back(&list, n1);
    mimi_list_push_back(&list, n2);
    mimi_list_push_back(&list, n3);

    /* pop all 3 nodes during safe traversal — each visited exactly once */
    mimi_node *node;
    mimi_node *tmp;
    int count = 0;
    mimi_list_for_each_safe_start(&list, node, tmp) {
        mimi_list_pop_front(&list);
        count++;
    } mimi_list_for_each_safe_end(&list, node, tmp);

    TEST_ASSERT_EQUAL_INT(3, count);
    TEST_ASSERT_NULL(list.head);

    free(n1); free(n2); free(n3);
}

static void test_null_checks(void)
{
    mimi_list list;
    mimi_list_init(&list);

    mimi_node *n = make_node();

    /* 非法入参现在由 mimi_assert 兜底: 期望的是断言被触发, 而不是返回错误码 */
    TEST_EXPECT_ASSERT(mimi_list_insert_back(NULL, n, n));
    TEST_EXPECT_ASSERT(mimi_list_insert_front(NULL, n, n));
    TEST_EXPECT_ASSERT(mimi_list_insert_back(&list, NULL, NULL));
    TEST_EXPECT_ASSERT(mimi_list_insert_front(&list, NULL, NULL));
    TEST_EXPECT_ASSERT(mimi_list_insert_back(&list, n, NULL));
    TEST_EXPECT_ASSERT(mimi_list_insert_front(&list, n, NULL));

    TEST_EXPECT_ASSERT(mimi_list_push_back(NULL, n));
    TEST_EXPECT_ASSERT(mimi_list_push_front(NULL, n));
    TEST_EXPECT_ASSERT(mimi_list_pop_front(NULL));
    TEST_EXPECT_ASSERT(mimi_list_rotate(NULL));
    TEST_EXPECT_ASSERT(mimi_list_remove(NULL, n));

    TEST_EXPECT_ASSERT(mimi_list_push_back(&list, NULL));
    TEST_EXPECT_ASSERT(mimi_list_push_front(&list, NULL));

    /* node 为空要先保证链表非空, 否则会先挂在 list->head != NULL 那条断言上 */
    mimi_list_push_back(&list, n);
    TEST_EXPECT_ASSERT(mimi_list_remove(&list, NULL));
    mimi_list_remove(&list, n);

    /* 空链表 pop / rotate: 断言 list->head != NULL */
    TEST_EXPECT_ASSERT(mimi_list_pop_front(&list));
    TEST_EXPECT_ASSERT(mimi_list_rotate(&list));

    free(n);
}

int run_list_tests(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init);
    RUN_TEST(test_node_reset_direct);
    RUN_TEST(test_push_back_one);
    RUN_TEST(test_push_back_multiple);
    RUN_TEST(test_push_front);
    RUN_TEST(test_insert_back);
    RUN_TEST(test_insert_back_after_tail);
    RUN_TEST(test_insert_front);
    RUN_TEST(test_pop_front);
    RUN_TEST(test_pop_back);
    RUN_TEST(test_rotate);
    RUN_TEST(test_rotate_single);
    RUN_TEST(test_reuse_after_empty);
    RUN_TEST(test_remove);
    RUN_TEST(test_remove_single);
    RUN_TEST(test_for_each_count);
    RUN_TEST(test_for_each_single);
    RUN_TEST(test_for_each_safe_remove_middle);
    RUN_TEST(test_for_each_safe_empty_after_pop);
    RUN_TEST(test_for_each_safe_pop_all);
    RUN_TEST(test_null_checks);

    return UNITY_END();
}
