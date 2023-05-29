/* bptreeIE.h */

extern int mybptreeIE_insertBTree (int table_id, void* element);
extern int mybptreeIE_eraseBTree(int table_id, void* key, short key_size);
extern void mybptreeIE_set_fn_key_test (int table_id,
                                     bool (*keyEl1_greater_keyEl2)(void*, void*), 
                                     bool (*keyEl1_equal_keyEl2  )(void*, void*),
                                     bool (*key1_greater_keyEl2  )(void*, void*),
                                     bool (*key1_equal_keyEl2  )(void*, void*));

extern void mybptreeIE_setup (int table_id, int element_size, int node_min, void* root);

