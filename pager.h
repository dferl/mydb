/* pager.h */

#define MYDB_PAGE_SIZE 4096
#define MYDB_CACHE_MAX_PAGES 1000

extern void mypager_open(const char* filename, const int TREE_ORDER);
extern struct page* mypager_get_page(int page_num, int level, bool new_page);
extern struct page* mypager_get_page_for_traverse(int page_num, int* index);
extern int mypager_get_unused_page_num(); 
extern void mypager_flush_cache_operation ();
extern void mypager_flush_free_cache();
extern void mypager_free_page (int i);
extern void mypager_close();
extern void mypager_mark_asToBeFlushed ();
extern short mypager_get_pageSize ();

struct page {
  int inPage_i;
  void* page_ptr;
};

struct pager {
  int file_descriptor;
  int file_length;
  int num_pages;
  int num_pages_in_file;
  int page_num[MYDB_CACHE_MAX_PAGES];
  struct page* pages[MYDB_CACHE_MAX_PAGES];
};

extern struct pager * g_pager;

