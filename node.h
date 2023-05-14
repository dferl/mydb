extern short mynode_get_node_order(void* node);
extern void mynode_set_node_order(void* node, short order);
extern short mynode_get_node_key_count(void* node);
extern short* mynode_get_node_key_count_addr(void* node);
extern void mynode_set_node_key_count(void* node, short key_count);
extern bool mynode_is_node_leaf(void* node);
extern void mynode_set_node_leaf(void* node, bool is_root);
extern int mynode_get_node_pages_count(void* node);
extern void mynode_set_node_pages_count(void* node, int pages_count);
extern int mynode_get_node_num_page(void* node);
extern void mynode_set_node_num_page(void* node, int num_page);
extern int mynode_get_node_el_key(void* node, int key_num);
extern void* mynode_get_node_el(void* node, int key_num);
extern void* mynode_get_node_child(void* node, short ind);
extern int mynode_get_node_child_page(void* node, short ind);
extern void mynode_set_node_child(void* node, short ind, void* childAddr);
extern int mynode_get_node_last_page_dumped(void* node);
extern void* mynode_get_node_last_addr_dumped(void* node);
extern void mynode_set_node_last_page_dumped(void* node, int last_page_dumped);
extern int mynode_get_node_num_pages_dumped(void* node);
extern void mynode_set_node_num_pages_dumped(void* node, int num_pages_dumped);
extern void* mynode_set_node_el(void* node, short ind, void* el);
extern void* mynode_set_node_elKey(void* node, short ind, void* elKey);
extern void* mynode_erase_node_el(void* node, short ind);
extern void* mynode_erase_node_elKey(void* node, short ind);
extern void mynode_set_fn_mark_asToBeFlushed ( void (*mypager_fn) (void*) ); 
extern void mynode_open_session(int element_size, short order, int key_size, const char* filename);
extern void mynode_close_session();
extern short mynode_get_max_order(int element_size, int key_size);
extern int mynode_get_node_leaf_link(void* node);
extern void mynode_set_node_leaf_link(void* node, int leaf_link);
extern void mynode_setNode (void* node, short key_count, bool leaf, int pages_count, int num_page, bool root);
extern void mynode_convert_to_root (void* nodeDest, void* nodeSource);
extern void mynode_convert_from_root (void* nodeDest, void* nodeSource, int num_page_dest);
extern void* mynode_get_root (bool* created);
extern void mynode_flush_cache_operation ();
extern void* mynode_get_node(void* nodeAddr, int level, bool new_page, bool internal, int* num_page_if_new);
extern void mynode_update_root_pages_count (void* node);
extern void* mynode_get_node_for_traverse_by_addr(void* nodeAddr, int* index);
extern void* mynode_get_node_for_traverse_by_page(int num_page, int* index);
extern void mynode_free_page (int index);
extern void mynode_erase_page (void* node);
extern void* mynode_set_node_child_from_node(void* nodeDest, short ind, void* nodeSource);
extern void* mynode_get_root_nodeAddr ();
extern int mynode_get_page_from_address(void* nodeAddr);
extern short mynode_get_section_from_address(void* nodeAddr);
extern void mynode_save_nodeAddr(void* nodeAddr);
extern void* mynode_get_saved_nodeAddr();
extern void mynode_dump_page (void* node);

