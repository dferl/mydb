/* bptreeS.h */

extern void* mybptreeS_search_key (int table_id, void* key, short key_size);
extern void mybptreeS_set_fn_key_test (int table_id,
                                     bool (*keyEl1_greater_keyEl2)(void*, void*), 
                                     bool (*keyEl1_equal_keyEl2  )(void*, void*),
                                     bool (*key1_greater_keyEl2  )(void*, void*),
                                     bool (*key1_equal_keyEl2  )(void*, void*));
extern void mybptreeS_setup();
extern int mybptreeS_open_cursor (int table_id, void* lowerKey, void* upperKey, int key_size, int element_size);
extern void* mybptreeS_fetch_cursor (int table_id, int cursor_id);
extern void mybptreeS_close_cursor (int table_id, int cursor_id);
extern int mybptreeS_open_cursor_xErase (int table_id, void* lowerKey, void* upperKey, int key_size, int element_size);
extern void* mybptreeS_fetch_cursor_xErase (int table_id, int cursor_id);
extern void mybptreeS_close_cursor_xErase (int table_id, int cursor_id);
extern int mybptreeS_update_element (int table_id, void* key, short key_size, void* element, short element_size);

