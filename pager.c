#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MYDB_CACHE_MAX_PAGES 10000
#define MYDB_CACHE1_MAX_PAGES 4000
#define MYDB_CACHE2_MAX_PAGES 4000
#define MYDB_CACHE3_MAX_PAGES 2000
#define MYDB_PAGE_SIZE 4096

struct pager {
  int file_descriptor;
  int file_length;
  int num_pages;
  int num_pages_in_file;
  bool page_to_flush[MYDB_CACHE_MAX_PAGES];
  int page_num[MYDB_CACHE_MAX_PAGES];
  void* pages[MYDB_CACHE_MAX_PAGES];
};

void mypager_open(const char* filename, const int TREE_ORDER);
void* mypager_get_page(int page_num, int level, bool new_page);
void* mypager_get_page_for_traverse(int page_num, int* index);
void mypager_flush_cache_operation ();
void mypager_flush_free_cache ();
void mypager_free_page (int i);
int mypager_get_unused_page_num(); 
void mypager_mark_asToBeFlushed ();
short mypager_get_pageSize ();
void mypager_close();
static void mypager_free_cache3 ();
static void mypager_flush(int page_num, void* page);
static int mydb_ipow(int base, int exp);

struct pager * g_pager;

static int g_max_level_top_cache;

void mypager_open(const char* filename, const int TREE_ORDER) {
    if (MYDB_CACHE_MAX_PAGES != MYDB_CACHE1_MAX_PAGES + MYDB_CACHE2_MAX_PAGES + MYDB_CACHE3_MAX_PAGES) {
        printf("cache sums different from total memory space. %d ", MYDB_CACHE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    int level, tot=0;
    for (level=0;; level++) {
        tot = tot + mydb_ipow (TREE_ORDER, level);
        if ( tot > MYDB_CACHE1_MAX_PAGES) break;
    }
    g_max_level_top_cache = level;

    int fd = open(filename,
        O_RDWR |      // Read/Write mode
        O_CREAT,  // Create file if it does not exist
        S_IWUSR |     // User write permission
        S_IRUSR   // User read permission
    );

    if (fd == -1) {
        printf("mypager_open - unable to open file\n");
        exit(EXIT_FAILURE);
    }

    int file_length = lseek(fd, 0, SEEK_END);

    g_pager = (struct pager*)malloc(sizeof(struct pager));
    g_pager->file_descriptor = fd;
    g_pager->file_length = file_length;
    g_pager->num_pages = (file_length / MYDB_PAGE_SIZE);
    g_pager->num_pages_in_file =  (file_length / MYDB_PAGE_SIZE);

    if (file_length % MYDB_PAGE_SIZE != 0) {
        printf("mypager_open - file is not a whole number of pages. Corrupt file.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < MYDB_CACHE_MAX_PAGES; i++) {
        g_pager->pages[i] = NULL;
        g_pager->page_num[i] = 0;
        g_pager->page_to_flush[i] = false;
    }
}

void mypager_close() {
    int result = close(g_pager->file_descriptor);
    if (result == -1) {
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }

    free(g_pager);
    g_pager=NULL;
}

void mypager_flush(int page_num, void* page) {
    int offset = lseek(g_pager->file_descriptor, page_num * MYDB_PAGE_SIZE, SEEK_SET);
 
    if (offset == -1) {
        printf("mypager_flush - error seeking, errno, page_num, page_num on node: %d, %d, %d\n", errno, page_num, *(int*)(page+2));
        exit(EXIT_FAILURE);
    }

//    printf ("mypager_flush, page %d, pos 32 int %d, page pointer %p \n", *(int*)(page+2), *(int*)(page+32), page);
 
    int index = *(int*)(page+4092);
    memset (page + 4092, '\0', 4);
 
    ssize_t bytes_written =
        write(g_pager->file_descriptor, page, MYDB_PAGE_SIZE);

    *(int*)(page+4092) = index; 

    if (bytes_written == -1) {
        printf("mypager_flush - error writing: %d\n", errno);
        exit(EXIT_FAILURE);
    }
}

void* mypager_get_page(int page_num, int level, bool new_page) {
  int i=0, iC1=0, iC2=0, iC3=9; 
 
  for (iC1=0; iC1<MYDB_CACHE1_MAX_PAGES && g_pager->pages[iC1] != NULL; iC1++) {
       if (page_num == g_pager->page_num[iC1]) {
           return g_pager->pages[iC1];
       }
  }
  for (iC2 = MYDB_CACHE1_MAX_PAGES; iC2 < MYDB_CACHE1_MAX_PAGES + MYDB_CACHE2_MAX_PAGES && g_pager->pages[iC2] != NULL; iC2++) {
       if (page_num == g_pager->page_num[iC2]) {
           return g_pager->pages[iC2];
       }
  }
  for (iC3 = MYDB_CACHE1_MAX_PAGES + MYDB_CACHE2_MAX_PAGES; 
                iC3 < MYDB_CACHE1_MAX_PAGES + MYDB_CACHE2_MAX_PAGES + MYDB_CACHE3_MAX_PAGES && g_pager->pages[iC3] != NULL; iC3++) {
       if (page_num == g_pager->page_num[iC3]) {
           return g_pager->pages[iC3];
       }
  }

  if (iC1 < MYDB_CACHE1_MAX_PAGES && level <= g_max_level_top_cache) {
    i=iC1;
  } else if (iC2 < MYDB_CACHE1_MAX_PAGES + MYDB_CACHE2_MAX_PAGES) {
    i=iC2;
  } else if (iC3 < MYDB_CACHE1_MAX_PAGES + MYDB_CACHE2_MAX_PAGES + MYDB_CACHE3_MAX_PAGES) {
    i=iC3;
  } else {
    printf("Tried to fetch page number out of bounds. %d > %d\n", page_num, MYDB_CACHE_MAX_PAGES);
    exit(EXIT_FAILURE);
  }

  void* page = malloc(MYDB_PAGE_SIZE);
  memset (page, '\0', MYDB_PAGE_SIZE);

  if (new_page) {
     g_pager->num_pages = g_pager->num_pages + 1;
  } else {
     int bytes_read=0;
     lseek(g_pager->file_descriptor, page_num * MYDB_PAGE_SIZE, SEEK_SET);
     bytes_read = read(g_pager->file_descriptor, page, MYDB_PAGE_SIZE);
     if (bytes_read == -1) {
        printf("Error reading file: %d\n", errno);
        exit(EXIT_FAILURE);
     }
  }

  *(int*)(page+4092) = i; 

  g_pager->pages[i] = page;
  g_pager->page_num[i] = page_num;

  return g_pager->pages[i];
}

void* mypager_get_page_for_traverse(int page_num, int* index) {
  int i=0;

  for (i=0; i<MYDB_CACHE_MAX_PAGES && g_pager->pages[i] != NULL; i++) {
       if (page_num == g_pager->page_num[i]) {
           *index = i;
           return g_pager->pages[i];
       }
  }
  if (i >= MYDB_CACHE_MAX_PAGES) {
    printf("Tried to fetch page number out of bounds. %d > %d\n", page_num,
           MYDB_CACHE_MAX_PAGES);
    exit(EXIT_FAILURE);
  }

  void* page = malloc(MYDB_PAGE_SIZE);
  memset (page, '\0', MYDB_PAGE_SIZE);

  int bytes_read=0;
  lseek(g_pager->file_descriptor, page_num * MYDB_PAGE_SIZE, SEEK_SET);
  bytes_read = read(g_pager->file_descriptor, page, MYDB_PAGE_SIZE);
  if (bytes_read == -1) {
     printf("Error reading file: %d\n", errno);
     exit(EXIT_FAILURE);
  }

  g_pager->pages[i] = page;
  g_pager->page_num[i] = page_num;

  *index = i;
  return g_pager->pages[i];
}

int mypager_get_unused_page_num() { 
  return g_pager->num_pages; 
}

void mypager_free_cache3 () {
  int iC3=0;
  for (iC3 = MYDB_CACHE1_MAX_PAGES + MYDB_CACHE2_MAX_PAGES; 
                iC3 < MYDB_CACHE1_MAX_PAGES + MYDB_CACHE2_MAX_PAGES + MYDB_CACHE3_MAX_PAGES && g_pager->pages[iC3] != NULL; iC3++) {
       free(g_pager->pages[iC3]);
       g_pager->pages[iC3] = NULL;
  }
}

void mypager_flush_cache_operation () {
  if (g_pager->pages[MYDB_CACHE1_MAX_PAGES + MYDB_CACHE2_MAX_PAGES] != NULL) {
     for (int i = 0; i < MYDB_CACHE_MAX_PAGES; i++) {
        if (g_pager->pages[i] != NULL) {
           if (g_pager->page_to_flush [i] = true) {
               mypager_flush(g_pager->page_num[i], g_pager->pages[i]);
               g_pager->page_to_flush [i] = false;
           }
        }
     }
     mypager_free_cache3();
  }
}

void mypager_flush_free_cache () {
  for (int i = 0; i < MYDB_CACHE_MAX_PAGES; i++) {
     if (g_pager->pages[i] != NULL) {
        if (g_pager->page_to_flush [i] = true) {
           mypager_flush(g_pager->page_num[i], g_pager->pages[i]);
           g_pager->page_to_flush [i] = false;
        }
        free(g_pager->pages[i]);
        g_pager->pages[i] = NULL;
     }
  }
}

void mypager_free_page (int i) {
  free(g_pager->pages[i]);
  g_pager->pages[i] = NULL;
}

void mypager_mark_asToBeFlushed (void* page) {
  int i = *(int*)(page + 4092); 
  g_pager->page_to_flush [i] = true; 
}

short mypager_get_pageSize () {
   return MYDB_PAGE_SIZE - 4;  
}

int mydb_ipow(int base, int exp)
{
    int result = 1;
    for (;;)
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }
    return result;
}





