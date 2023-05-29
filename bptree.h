/* bptree.h */

extern int mybptree_insertBTree (int table_id, void* element);
extern int mybptree_openBTree(const char* filename, int element_size, short order, short key_size, bool readOnly);
extern void mybptree_closeBTree(int table_id);
extern short mybptree_get_max_order(int element_size, int key_size);
extern int mybptree_eraseBTree(int table_id, void* key, short key_size);
extern void mybptree_set_fn_key_test (int table_id,
                                     bool (*keyEl1_greater_keyEl2)(void*, void*), 
                                     bool (*keyEl1_equal_keyEl2  )(void*, void*),
                                     bool (*key1_greater_keyEl2  )(void*, void*),
                                     bool (*key1_equal_keyEl2  )(void*, void*));
extern void* mybptree_search_key (int table_id, void* key, short key_size);
extern int mybptree_open_cursor (int table_id, void* lowerKey, void* upperKey, int key_size, int element_size);
extern void* mybptree_fetch_cursor (int table_id, int cursor_id);
extern void mybptree_close_cursor (int table_id, int cursor_id);
extern int mybptree_open_cursor_xErase (int table_id, void* lowerKey, void* upperKey, int key_size, int element_size);
extern void* mybptree_fetch_cursor_xErase (int table_id, int cursor_id);
extern void mybptree_close_cursor_xErase (int table_id, int cursor_id);
extern int mybptree_update_element (int table_id, void* key, short key_size, void* element, short element_size);
