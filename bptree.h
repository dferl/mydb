/* bptree.h */

extern int mybptree_insertBTree (void* element);
extern void mybptree_openBTree(const char* filename, int element_size, short order, short key_size);
extern void mybptree_closeBTree();
extern int mybptree_traverseBTree ();
extern void* mybptree_search_key (void* key, short key_size);
extern short mybptree_get_max_order(int element_size, int key_size);
extern int mybptree_eraseBTree(void* key, short key_size);
extern void mybptree_set_fn_key_test (bool (*keyEl1_greater_keyEl2)(void*, void*), 
                                     bool (*keyEl1_equal_keyEl2  )(void*, void*),
                                     bool (*key1_greater_keyEl2  )(void*, void*),
                                     bool (*key1_equal_keyEl2  )(void*, void*));
