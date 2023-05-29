#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define CACHE_MAX_PAGES 10000
#define CACHE1_MAX_PAGES 4000
#define CACHE2_MAX_PAGES 4000
#define CACHE3_MAX_PAGES 2000
#define PAGE_SIZE 4096
#define MAX_PAGERS 10

typedef unsigned int page_type;
typedef unsigned int count_type;
typedef short section_type;

struct pager {
  int file_descriptor;
  int file_length;
  char filename [50];
  bool readOnly;
  count_type num_pages;
  bool page_to_flush[CACHE_MAX_PAGES];
  page_type page_num[CACHE_MAX_PAGES];
  void* pages[CACHE_MAX_PAGES];
};

static void mypager_free_cache3 (int table_id);
static void mypager_flush(int table_id, page_type page_num, void* page);
static int mydb_ipow(int base, int exp);
static void fileCopy(const char* filenameInput, const char* filenameOutput);

struct pager* g_pager [MAX_PAGERS];

static int g_max_level_top_cache[MAX_PAGERS];
static void* g_page;
static int g_fd;

int mypager_open(const char* filename, const int TREE_ORDER, bool readOnly) {
    if (CACHE_MAX_PAGES != CACHE1_MAX_PAGES + CACHE2_MAX_PAGES + CACHE3_MAX_PAGES) {
        printf("cache sums different from total memory space. %d ", CACHE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    if (sizeof(filename) > 94) {
        printf("file name too long \n");
        exit(EXIT_FAILURE);
    }

    int i;
    for (i=0; *(char*)(filename+i) != '\0'; i++);

    int fd;
    if (readOnly) {
        fd = open(filename,
            O_RDWR |      // Read/Write mode
            O_CREAT,  // Create file if it does not exist
            S_IWUSR |     // User write permission
            S_IRUSR   // User read permission
        );
        if (fd == -1) {
            printf("mypager_open - unable to open file %s \n", filename);
            exit(EXIT_FAILURE);
        }
    } else {
        char working_filename [50];
        memset (working_filename, '\0', sizeof(working_filename));
        memcpy (working_filename, filename, i);
        strcat (working_filename, ".bkp");
        fileCopy(filename, working_filename);

        fd = open (working_filename,
            O_RDWR |      // Read/Write mode
            O_CREAT,  // Create file if it does not exist
            S_IWUSR |     // User write permission
            S_IRUSR   // User read permission
        );
        if (fd == -1) {
            printf("mypager_open - unable to open file %s \n", working_filename);
            exit(EXIT_FAILURE);
        }
    }

    int file_length = lseek(fd, 0, SEEK_END);

    int iPager;
    for (iPager=0; iPager<MAX_PAGERS && g_pager[iPager] != NULL; iPager++);
    if (iPager==MAX_PAGERS) {
        printf("Too many tables opened at the same time \n");
        exit(EXIT_FAILURE);
    }
    g_pager[iPager] = (struct pager*)malloc(sizeof(struct pager));
    g_pager[iPager]->file_descriptor = fd;
    g_pager[iPager]->file_length = file_length;
    g_pager[iPager]->num_pages = (file_length / PAGE_SIZE);
    g_pager[iPager]->readOnly = readOnly;
    memcpy (g_pager[iPager]->filename, filename, i);
    g_fd = fd;

    if (file_length % PAGE_SIZE != 0) {
        printf("mypager_open - file is not a whole number of pages. Corrupt file.\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < CACHE_MAX_PAGES; i++) {
        g_pager[iPager]->pages[i] = NULL;
        g_pager[iPager]->page_num[i] = 0;
        g_pager[iPager]->page_to_flush[i] = false;
    }

    g_page = malloc(PAGE_SIZE);


    int level, tot=0;
    for (level=0;; level++) {
        tot = tot + mydb_ipow (TREE_ORDER, level);
        if ( tot > CACHE1_MAX_PAGES) break;
    }
    g_max_level_top_cache[iPager] = level;

    return iPager;
}

void mypager_open_only_read_no_cache(const char* filename) {
    if (sizeof(filename) > 94) {
        printf("file name too long \n");
        exit(EXIT_FAILURE);
    }
    g_fd = open(filename,
         O_RDWR |      // Read/Write mode
         O_CREAT,  // Create file if it does not exist
         S_IWUSR |     // User write permission
         S_IRUSR   // User read permission
    );
    if (g_fd == -1) {
        printf("mypager_open - unable to open file %s \n", filename);
        exit(EXIT_FAILURE);
    }
    int file_length = lseek(g_fd, 0, SEEK_END);
    if (file_length == 0) {
        printf("mypager_open only read - the table file is empty %s \n", filename);
        exit(EXIT_FAILURE);
    }

    g_page = malloc(PAGE_SIZE);
}

void mypager_close(int table_id) {
    int result = close(g_pager[table_id]->file_descriptor);
    if (result == -1) {
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }

    if (!g_pager[table_id]->readOnly) {
        int i;
        for (i=0; *(char*)((g_pager[table_id]->filename)+i) != '\0'; i++);
        char working_filename [50];
        memset (working_filename, '\0', sizeof(working_filename));
        memcpy (working_filename, g_pager[table_id]->filename, i);
        strcat (working_filename, ".bkp");

        fileCopy(working_filename, g_pager[table_id]->filename);
        result = remove(working_filename);
        if (result == -1) {
            printf("Error in removing working file.\n");
            exit(EXIT_FAILURE);
        }
    }

    free(g_pager[table_id]);
    g_pager[table_id]=NULL;
}

void mypager_close_only_read_no_cache() {
    int result = close(g_fd);
    if (result == -1) {
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }
}

void* mypager_get_page(int table_id, page_type page_num, int level, bool new_page, int* cache_index) {
  int i=0, iC1=0, iC2=0, iC3=9; 
 
  for (iC1=0; iC1<CACHE1_MAX_PAGES && g_pager[table_id]->pages[iC1] != NULL; iC1++) {
       if (page_num == g_pager[table_id]->page_num[iC1]) {
           *cache_index = iC1;
           return g_pager[table_id]->pages[iC1];
       }
  }
  for (iC2 = CACHE1_MAX_PAGES; iC2 < CACHE1_MAX_PAGES + CACHE2_MAX_PAGES && g_pager[table_id]->pages[iC2] != NULL; iC2++) {
       if (page_num == g_pager[table_id]->page_num[iC2]) {
           *cache_index = iC2;
           return g_pager[table_id]->pages[iC2];
       }
  }
  for (iC3 = CACHE1_MAX_PAGES + CACHE2_MAX_PAGES; 
                iC3 < CACHE1_MAX_PAGES + CACHE2_MAX_PAGES + CACHE3_MAX_PAGES && g_pager[table_id]->pages[iC3] != NULL; iC3++) {
       if (page_num == g_pager[table_id]->page_num[iC3]) {
           *cache_index = iC3;
           return g_pager[table_id]->pages[iC3];
       }
  }

  if (iC1 < CACHE1_MAX_PAGES && level <= g_max_level_top_cache[table_id]) {
    i=iC1;
  } else if (iC2 < CACHE1_MAX_PAGES + CACHE2_MAX_PAGES) {
    i=iC2;
  } else if (iC3 < CACHE1_MAX_PAGES + CACHE2_MAX_PAGES + CACHE3_MAX_PAGES) {
    i=iC3;
  } else {
    printf("Tried to fetch page number out of bounds. %d > %d\n", page_num, CACHE_MAX_PAGES);
    exit(EXIT_FAILURE);
  }

  void* page = malloc(PAGE_SIZE);
  memset (page, '\0', PAGE_SIZE);

  if (new_page) {
     g_pager[table_id]->num_pages = g_pager[table_id]->num_pages + 1;
  } else {
     int bytes_read=0;
     lseek(g_pager[table_id]->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
     bytes_read = read(g_pager[table_id]->file_descriptor, page, PAGE_SIZE);
     if (bytes_read == -1) {
        printf("Error reading file: %d\n", errno);
        exit(EXIT_FAILURE);
     }
  }

  *cache_index = i;

  g_pager[table_id]->pages[i] = page;
  g_pager[table_id]->page_num[i] = page_num;

  return g_pager[table_id]->pages[i];
}

void* mypager_get_page_no_caching(int table_id, page_type page_num, int* index) {
  *index = 0;
  int iC1=0, iC2=0, iC3=9; 

  for (iC1=0; iC1<CACHE1_MAX_PAGES && g_pager[table_id]->pages[iC1] != NULL; iC1++) {
       if (page_num == g_pager[table_id]->page_num[iC1]) {
           return g_pager[table_id]->pages[iC1];
       }
  }
  for (iC2 = CACHE1_MAX_PAGES; iC2 < CACHE1_MAX_PAGES + CACHE2_MAX_PAGES && g_pager[table_id]->pages[iC2] != NULL; iC2++) {
       if (page_num == g_pager[table_id]->page_num[iC2]) {
           return g_pager[table_id]->pages[iC2];
       }
  }
  for (iC3 = CACHE1_MAX_PAGES + CACHE2_MAX_PAGES; 
                iC3 < CACHE1_MAX_PAGES + CACHE2_MAX_PAGES + CACHE3_MAX_PAGES && g_pager[table_id]->pages[iC3] != NULL; iC3++) {
       if (page_num == g_pager[table_id]->page_num[iC3]) {
           return g_pager[table_id]->pages[iC3];
       }
  }

  memset (g_page, '\0', PAGE_SIZE);

  int bytes_read=0;
  lseek(g_pager[table_id]->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
  bytes_read = read(g_pager[table_id]->file_descriptor, g_page, PAGE_SIZE);
  if (bytes_read == -1) {
     printf("Error reading file: %d\n", errno);
     exit(EXIT_FAILURE);
  }

  return g_page;
}

void* mypager_get_page_for_traverse(page_type page_num, int* index) {
  memset (g_page, '\0', PAGE_SIZE);

  int bytes_read=0;
  lseek(g_fd, page_num * PAGE_SIZE, SEEK_SET);
  bytes_read = read(g_fd, g_page, PAGE_SIZE);
  if (bytes_read == -1) {
     printf("Error reading file: %d\n", errno);
     exit(EXIT_FAILURE);
  }

  *index = 0;
  return g_page;
}

page_type mypager_get_unused_page_num(int table_id) { 
  return g_pager[table_id]->num_pages; 
}

void mypager_flush(int table_id, page_type page_num, void* page) {
    int offset = lseek(g_pager[table_id]->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
    if (offset == -1) {
        printf("mypager_flush - error seeking, errno, page_num, page_num on node: %d, %d, %d\n", errno, page_num, *(int*)(page+2));
        exit(EXIT_FAILURE);
    }
 
    ssize_t bytes_written =
        write(g_pager[table_id]->file_descriptor, page, PAGE_SIZE);

    if (bytes_written == -1) {
        printf("mypager_flush - error writing: %d\n", errno);
        exit(EXIT_FAILURE);
    }
}

void mypager_free_cache3 (int table_id) {
  int iC3=0;
  for (iC3 = CACHE1_MAX_PAGES + CACHE2_MAX_PAGES; 
                iC3 < CACHE1_MAX_PAGES + CACHE2_MAX_PAGES + CACHE3_MAX_PAGES && g_pager[table_id]->pages[iC3] != NULL; iC3++) {
       free(g_pager[table_id]->pages[iC3]);
       g_pager[table_id]->pages[iC3] = NULL;
  }
}

void mypager_flush_cache_operation (int table_id) {
  if (g_pager[table_id]->pages[CACHE1_MAX_PAGES + CACHE2_MAX_PAGES] != NULL) {
     for (int i = 0; i < CACHE_MAX_PAGES; i++) {
        if (g_pager[table_id]->pages[i] != NULL) {
           if (g_pager[table_id]->page_to_flush [i] = true) {
               mypager_flush (table_id, g_pager[table_id]->page_num[i], g_pager[table_id]->pages[i]);
               g_pager[table_id]->page_to_flush [i] = false;
           }
        }
     }
     mypager_free_cache3(table_id);
  }
}

void mypager_flush_free_cache (int table_id) {
  for (int i = 0; i < CACHE_MAX_PAGES; i++) {
     if (g_pager[table_id]->pages[i] != NULL) {
        if (g_pager[table_id]->page_to_flush [i] = true) {
           mypager_flush(table_id, g_pager[table_id]->page_num[i], g_pager[table_id]->pages[i]);
           g_pager[table_id]->page_to_flush [i] = false;
        }
        free(g_pager[table_id]->pages[i]);
        g_pager[table_id]->pages[i] = NULL;
     }
  }
}

void mypager_free_page (int table_id, int i) {
  free(g_pager[table_id]->pages[i]);
  g_pager[table_id]->pages[i] = NULL;
}

void mypager_mark_asToBeFlushed (int table_id, int cache_index) {
  g_pager[table_id]->page_to_flush [cache_index] = true; 
}

int mypager_get_pageSize () {
   return PAGE_SIZE;  
}

count_type mypager_get_num_pages(int table_id) {
    return g_pager[table_id]->num_pages;
}

void fileCopy(const char* filenameInput, const char* filenameOutput)
{
    FILE *fpI;
    FILE *fpO;    
    char buffer[4096];
    size_t n;

    fpI = fopen( filenameInput , "r" );
    if (!fpI) return; 
    fpO = fopen( filenameOutput , "w" );

    while ( (n = fread(buffer, sizeof(char), sizeof(buffer), fpI))  > 0)
    {
        if (fwrite(buffer, sizeof(char), n, fpO) != n) {
            printf("write failed\n");
            exit(EXIT_FAILURE);
        }
    }

    fclose( fpI );
    fclose( fpO );
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

