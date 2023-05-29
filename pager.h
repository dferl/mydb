/* pager.h */

typedef unsigned int page_type;
typedef unsigned int count_type;
typedef short section_type;

extern int mypager_open(const char* filename, const int TREE_ORDER, bool readOnly);
extern void mypager_close(int table_id);
extern struct page* mypager_get_page(int table_id, page_type page_num, int level, bool new_page, int* cache_index);
extern void* mypager_get_page_no_caching(int table_id, page_type page_num, int* index);
extern struct page* mypager_get_page_for_traverse(page_type page_num, int* index);
extern page_type mypager_get_unused_page_num(int table_id); 
extern void mypager_flush_cache_operation (int table_id);
extern void mypager_flush_free_cache(int table_id);
extern void mypager_free_page (int table_id, int i);
extern void mypager_mark_asToBeFlushed (int table_id, int cache_index);
extern int mypager_get_pageSize ();
extern count_type mypager_get_num_pages(int table_id);
extern void mypager_open_only_read_no_cache(const char* filename);
extern void mypager_close_only_read_no_cache();

