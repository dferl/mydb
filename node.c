#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "pager.h"

typedef unsigned int page_type;
typedef unsigned int count_type;
typedef short section_type;

#define MAX_TABLES 10

/*
 * Common Node Block Layout
 */
#define NODE_COMMON_BLOCK_OFFSET 0
#define NODE_LEAF_OFFSET 0
#define NODE_LEAF_SIZE sizeof(short)
#define NODE_NUM_PAGE_OFFSET NODE_LEAF_OFFSET + NODE_LEAF_SIZE
#define NODE_NUM_PAGE_SIZE sizeof(page_type)
#define NODE_EL_COUNT_OFFSET NODE_NUM_PAGE_OFFSET + NODE_NUM_PAGE_SIZE
#define NODE_EL_COUNT_SIZE sizeof(count_type)
#define NODE_LAST_PAGE_INT_DUMPED_OFFSET NODE_EL_COUNT_OFFSET + NODE_EL_COUNT_SIZE
#define NODE_LAST_PAGE_INT_DUMPED_SIZE sizeof(page_type)
#define NODE_LAST_SECT_INT_DUMPED_OFFSET NODE_LAST_PAGE_INT_DUMPED_OFFSET + NODE_LAST_PAGE_INT_DUMPED_SIZE
#define NODE_LAST_SECT_INT_DUMPED_SIZE sizeof(section_type)
#define NODE_LAST_PAGE_LEAF_DUMPED_OFFSET NODE_LAST_SECT_INT_DUMPED_OFFSET + NODE_LAST_SECT_INT_DUMPED_SIZE
#define NODE_LAST_PAGE_LEAF_DUMPED_SIZE sizeof(page_type)
#define NODE_INTERNAL_BLOCK_SIZE_OFFSET NODE_LAST_PAGE_LEAF_DUMPED_OFFSET + NODE_LAST_PAGE_LEAF_DUMPED_SIZE
#define NODE_INTERNAL_BLOCK_SIZE_SIZE sizeof(int)
#define NODE_LINK_PAGE_OFFSET NODE_INTERNAL_BLOCK_SIZE_OFFSET + NODE_INTERNAL_BLOCK_SIZE_SIZE
#define NODE_LINK_PAGE_SIZE sizeof(page_type)
#define NODE_LINK_SECT_OFFSET NODE_LINK_PAGE_OFFSET + NODE_LINK_PAGE_SIZE
#define NODE_LINK_SECT_SIZE sizeof(section_type)
#define NODE_COMMON_BLOCK_SIZE NODE_LEAF_SIZE + \
                               NODE_NUM_PAGE_SIZE + \
                               NODE_EL_COUNT_SIZE + \
                               NODE_LAST_PAGE_INT_DUMPED_SIZE + \
                               NODE_LAST_SECT_INT_DUMPED_SIZE + \
                               NODE_LAST_PAGE_LEAF_DUMPED_SIZE + \
                               NODE_INTERNAL_BLOCK_SIZE_SIZE + \
                               NODE_LINK_PAGE_SIZE + \
                               NODE_LINK_SECT_SIZE \
/*
 * Root Node Block Layout 
 */
#define NODE_ROOT_BLOCK_OFFSET NODE_COMMON_BLOCK_SIZE
#define NODE_ORDER_OFFSET NODE_COMMON_BLOCK_SIZE
#define NODE_ORDER_SIZE sizeof(short)
#define NODE_NUM_PAGES_DUMPED_INT_OFFSET NODE_ORDER_OFFSET + NODE_ORDER_SIZE
#define NODE_NUM_PAGES_DUMPED_INT_SIZE sizeof(count_type)
#define NODE_NUM_PAGES_DUMPED_LEAF_OFFSET NODE_NUM_PAGES_DUMPED_INT_OFFSET + NODE_NUM_PAGES_DUMPED_INT_SIZE
#define NODE_NUM_PAGES_DUMPED_LEAF_SIZE sizeof(count_type)
#define NODE_NUM_PAGES_OFFSET NODE_NUM_PAGES_DUMPED_LEAF_OFFSET + NODE_NUM_PAGES_DUMPED_LEAF_SIZE
#define NODE_NUM_PAGES_SIZE sizeof(count_type)
#define NODE_LAST_PAGE_INTERNAL_STORE_OFFSET NODE_NUM_PAGES_OFFSET + NODE_NUM_PAGES_SIZE
#define NODE_LAST_PAGE_INTERNAL_STORE_SIZE sizeof(page_type)
#define NODE_LAST_SECT_INTERNAL_STORE_OFFSET NODE_LAST_PAGE_INTERNAL_STORE_OFFSET + NODE_LAST_PAGE_INTERNAL_STORE_SIZE
#define NODE_LAST_SECT_INTERNAL_STORE_SIZE sizeof(section_type)
#define NODE_PAGE_SIZE_OFFSET NODE_LAST_SECT_INTERNAL_STORE_OFFSET + NODE_LAST_SECT_INTERNAL_STORE_SIZE
#define NODE_PAGE_SIZE_SIZE sizeof(int)
#define NODE_ROOT_BLOCK_SIZE NODE_ORDER_SIZE + \
                                 NODE_NUM_PAGES_DUMPED_INT_SIZE + \
                                 NODE_NUM_PAGES_DUMPED_LEAF_SIZE + \
                                 NODE_NUM_PAGES_SIZE + \
                                 NODE_LAST_PAGE_INTERNAL_STORE_SIZE + \
                                 NODE_LAST_SECT_INTERNAL_STORE_SIZE + \
                                 NODE_PAGE_SIZE_SIZE
#define NODE_ROOT_CHILDREN_BLOCK_OFFSET NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE
#define NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET (NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE)

static int NODE_ROOT_INTERNAL_ELEMENT_BLOCK_OFFSET[MAX_TABLES];

/*
 * Internal Node Block Layout
 */
#define NODE_INTERNAL_BLOCK_OFFSET NODE_COMMON_BLOCK_SIZE
#define NODE_INTERNAL_BLOCK_SIZE 0
#define NODE_INTERNAL_CHILDREN_BLOCK_OFFSET NODE_INTERNAL_BLOCK_OFFSET + NODE_INTERNAL_BLOCK_SIZE
static int NODE_INTERNAL_ELEMENT_BLOCK_OFFSET[MAX_TABLES];
static int NODE_INTERNAL_NODE_BLOCK_SIZE[MAX_TABLES];

/*
 * Leaf Node Block Layout
 */
#define NODE_LEAF_BLOCK_OFFSET NODE_COMMON_BLOCK_SIZE
#define NODE_LEAF_BLOCK_SIZE 0
#define NODE_LEAF_ELEMENT_BLOCK_OFFSET NODE_LEAF_BLOCK_OFFSET + NODE_LEAF_BLOCK_SIZE

#define NODE_ADDR_SIZE (sizeof(page_type) + sizeof(section_type))

/*
 * Root/Internal/Leaf Node Variable parts Layout
 */
#define SIZE_OF_PAGE sizeof(page_type)
#define SIZE_OF_SECTION sizeof(section_type)
#define LEAF_SECTION_DEFAULT_NUMBER 9999

static int PAGE_SIZE;
static int NODE_ELEMENT_SIZE[MAX_TABLES];
static int NODE_KEY_SIZE[MAX_TABLES];
static short TREE_ORDER[MAX_TABLES];
static short PAGE_INT_NODE_CAPACITY[MAX_TABLES];

struct node {
  int  nodeSize;
  section_type pageSection;
  int cacheIndex;
  void* nodePtr;
  void* page;
};

void mynode_set_node_order(void* node, short order);
void mynode_set_node_page_size(void* node, int page_size);
int mynode_get_node_page_size(void* node);
count_type mynode_get_node_key_count(void* node);
void mynode_set_node_key_count(void* node, count_type key_count);
bool mynode_is_node_leaf(void* node);
void mynode_set_node_leaf(void* node, bool is_root);
count_type mynode_get_node_pages_count(void* node);
void mynode_set_node_pages_count(void* node, count_type pages_count);
page_type mynode_get_node_num_page(void* node);
void mynode_set_node_num_page(void* node, page_type num_page, bool defrag);
void* mynode_get_node_el(void* node, count_type key_num);
void* mynode_get_node_child(void* node, count_type ind);
count_type mynode_get_node_child_page(void* node, count_type ind);
void mynode_set_node_child(void* node, count_type ind, void* childAddr);
void* mynode_set_node_el(void* node, count_type ind, void* el);
void* mynode_set_node_elKey(void* node, count_type ind, void* elKey);
void* mynode_erase_node_el(void* node, count_type ind);
void* mynode_erase_node_elKey(void* node, count_type ind);
short mynode_get_max_order(int element_size, int key_size);
void mynode_convert_to_root (void* nodeDest, void* nodeSource);
void mynode_convert_from_root (void* nodeDest, void* nodeSource, count_type num_page_dest);
void* mynode_get_root (bool* created);
void mynode_flush_cache_operation ();
void* mynode_get_node(void* nodeAddr, int level, bool new_node, bool internal, page_type* num_page_if_new);
void mynode_update_root_pages_count (void* node);
void mynode_free_page (int index);
void mynode_erase_page (void* node);
void* mynode_get_root_nodeAddr ();
page_type mynode_get_page_from_address(void* nodeAddr);
section_type mynode_get_section_from_address(void* nodeAddr);
void mynode_save_nodeAddr(void* nodeAddr);
void* mynode_get_saved_nodeAddr();
void mynode_save_nodeAddr1(void* nodeAddr);
void* mynode_get_saved_nodeAddr1();
void mynode_dump_page (void* node);
void* mynode_get_node_link(void* node);
void mynode_set_node_link(void* node, void* address);
void mynode_set_node_link_with_nodeAddr(void* nodeDest, void* nodeSource);
void mynode_resetRoot (void* node, count_type key_count, bool leaf, count_type pages_count, page_type num_page);
int mynode_get_internal_block(void* node);
page_type mynode_get_node_link_page(void* node);
page_type mynode_get_page_last_leaf_dumped(void* node);
count_type mynode_get_node_num_pages_dumped_int(void* node);
void* mynode_get_addr_last_int_dumped (void* node);

static void mynode_set_node_num_pages_dumped_int(void* node, count_type num_pages_dumped, bool defrag);
static count_type mynode_get_node_num_pages_dumped_leaf(void* node);
static void mynode_set_node_num_pages_dumped_leaf(void* node, count_type num_pages_dumped, bool defrag);
static void mynode_set_addr_last_int_dumped (void* node, void* nodeAddr, bool defrag);
static void mynode_set_page_last_leaf_dumped(void* node, page_type last_page_dumped, bool defrag);
static void mynode_set_last_sect_internal_store(void* node, section_type page);
static section_type mynode_get_last_sect_internal_store(void* node);
static void mynode_set_last_page_internal_store(void* node, page_type page);
static page_type mynode_get_last_page_internal_store(void* node);
static page_type mynode_get_unused_page_leaf (int level);
static void mynode_get_unused_page_int (int level, bool* recycled);
static void mynode_clear_nodes_table ();
static void* mynode_get_empty_nodeAddr();
static void mynode_set_internal_block(void* node, int blockSize);
static void mynode_init_node_link(void* node);
static void* mynode_get_node_by_addr_no_caching(void* nodeAddr, int* index);
static void mynode_init_g_variables (int table_id, int element_size, short order, int key_size);

static struct node* g_node;
static void* g_nodeAddr;
static void* g_nodeAddr1;
static void* g_nodeAddr2;
static int g_table_id;
static bool read_no_cache_mode_session=false;

#define MYDB_MAX_NODES 1000
struct node g_nodes [MAX_TABLES][MYDB_MAX_NODES];
static void* g_root[MAX_TABLES];
static int g_nodes_index[MAX_TABLES]={0,0,0,0,0,0,0,0,0,0};
static page_type g_last_page_internal_store[MAX_TABLES]={0,0,0,0,0,0,0,0,0,0};
static section_type g_last_sect_internal_store[MAX_TABLES]={0,0,0,0,0,0,0,0,0,0};

void mynode_set_table_id (int table_id) {
  g_table_id = table_id;
}

int mynode_open_session(int element_size, short order, int key_size, const char* filename, bool readOnly) {
  if (read_no_cache_mode_session) {
     printf("Session in read mode/no cache mode active; session in any other mode cannot be started\n"); 
     exit(EXIT_FAILURE);
  }

  int table_id = mypager_open(filename, order, readOnly);

  mynode_init_g_variables (table_id, element_size, order, key_size);

  g_nodes_index[table_id] = 0;
  for (int i=0; i<MYDB_MAX_NODES; i++) g_nodes[table_id][i].page=NULL;

  g_root[table_id] = malloc (sizeof (struct node));
  if (!g_nodeAddr) g_nodeAddr = malloc (NODE_ADDR_SIZE);
  if (!g_nodeAddr1) g_nodeAddr1 = malloc (NODE_ADDR_SIZE);
  if (!g_nodeAddr2) g_nodeAddr2 = malloc (NODE_ADDR_SIZE);
  if (!g_node) g_node = malloc (sizeof (struct node));
  return table_id;
}

void mynode_open_session_only_read_no_cache(int element_size, short order, int key_size, const char* filename) {
  read_no_cache_mode_session=true;
  int i;
  for (i=0; i<MAX_TABLES && g_nodes[i][0].page==NULL; i++);
  if (i!=MAX_TABLES) {
     printf("Tables in use; session in read mode/no cache mode cannot be started\n"); 
     exit(EXIT_FAILURE);
  }

  mypager_open_only_read_no_cache(filename);

  mynode_init_g_variables (0, element_size, order, key_size);

  g_nodeAddr = malloc (NODE_ADDR_SIZE);
  g_nodeAddr1 = malloc (NODE_ADDR_SIZE);
  g_nodeAddr2 = malloc (NODE_ADDR_SIZE);
  g_root[0] = malloc (sizeof (struct node));
  g_node = malloc (sizeof (struct node));

  mynode_set_table_id(0);
}

void mynode_init_g_variables (int table_id, int element_size, short order, int key_size) {
  NODE_ELEMENT_SIZE[table_id] = element_size;
  TREE_ORDER[table_id] = order;
  PAGE_SIZE = mypager_get_pageSize();
  NODE_KEY_SIZE[table_id] = key_size;
  NODE_ROOT_INTERNAL_ELEMENT_BLOCK_OFFSET[table_id] = NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE + (NODE_ADDR_SIZE * (TREE_ORDER[table_id] + 1));
  NODE_INTERNAL_ELEMENT_BLOCK_OFFSET[table_id] = NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + (NODE_ADDR_SIZE * (TREE_ORDER[table_id] + 1));

  if (NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET + TREE_ORDER[table_id] * NODE_ELEMENT_SIZE[table_id] > PAGE_SIZE) {
      printf("too many elements per leaf page; page size %d, tree_order %d, element size %d, leaf block %ld \n", PAGE_SIZE, TREE_ORDER[table_id], 
                                                                                                                 NODE_ELEMENT_SIZE[table_id], 
                                                                                                                 NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET);
      exit(EXIT_FAILURE);
  }

  if (NODE_ROOT_INTERNAL_ELEMENT_BLOCK_OFFSET[table_id] + TREE_ORDER[table_id] * NODE_KEY_SIZE[table_id] > PAGE_SIZE) {
      printf("too many elements per internal page; page size %d, tree_order %d, key size %d, num page size %ld, internal block %d \n", 
                                                                                                                PAGE_SIZE,
                                                                                                                TREE_ORDER[table_id],
                                                                                                                NODE_ELEMENT_SIZE[table_id],
                                                                                                                NODE_ADDR_SIZE,
                                                                                                                NODE_ROOT_INTERNAL_ELEMENT_BLOCK_OFFSET[table_id]);
      exit(EXIT_FAILURE);
  }

  NODE_INTERNAL_NODE_BLOCK_SIZE[table_id] = NODE_INTERNAL_ELEMENT_BLOCK_OFFSET[table_id] +  TREE_ORDER[table_id] * NODE_KEY_SIZE[table_id];

  PAGE_INT_NODE_CAPACITY[table_id] = PAGE_SIZE / NODE_INTERNAL_NODE_BLOCK_SIZE[table_id];
  if (PAGE_INT_NODE_CAPACITY[table_id] > LEAF_SECTION_DEFAULT_NUMBER) PAGE_INT_NODE_CAPACITY[table_id] = LEAF_SECTION_DEFAULT_NUMBER - 1;

}

void mynode_close_session() {
  mynode_set_last_page_internal_store (g_root[g_table_id], g_last_page_internal_store[g_table_id]);
  mynode_set_last_sect_internal_store (g_root[g_table_id], g_last_sect_internal_store[g_table_id]);
  mypager_flush_free_cache (g_table_id);
  mypager_close(g_table_id);
  free (g_nodeAddr);
  g_nodeAddr=NULL;
  free (g_nodeAddr1);
  g_nodeAddr1=NULL;
  free (g_nodeAddr2);
  g_nodeAddr2=NULL;
  free (g_root[g_table_id]);
  g_root[g_table_id]=NULL;
}

void mynode_close_session_read_no_cache() {
  read_no_cache_mode_session=false;
  mypager_close_only_read_no_cache();
  free (g_nodeAddr);
  g_nodeAddr=NULL;
  free (g_nodeAddr1);
  g_nodeAddr1=NULL;
  free (g_nodeAddr2);
  g_nodeAddr2=NULL;
  free(g_root[0]);
  g_root[0]=NULL;
  free(g_node);
  g_node=NULL;
}

void mynode_flush_cache_operation () {
  mypager_flush_cache_operation (g_table_id);
  mynode_clear_nodes_table();
}

void* mynode_get_root (bool* created) {
  int cache_index;
  int i = g_nodes_index[g_table_id];
  if (mypager_get_num_pages(g_table_id) == 0) {
      *created = true;
      g_nodes [g_table_id][i].page = mypager_get_page(g_table_id, 0, 0, true, &cache_index);
  } else {
      *created = false;
      g_nodes [g_table_id][i].page = mypager_get_page(g_table_id, 0, 0, false, &cache_index);
  }
  
  g_nodes [g_table_id][i].pageSection = LEAF_SECTION_DEFAULT_NUMBER;
  g_nodes [g_table_id][i].nodeSize = PAGE_SIZE;
  g_nodes [g_table_id][i].nodePtr = g_nodes [g_table_id][i].page;
  g_nodes [g_table_id][i].cacheIndex = cache_index;
  memcpy (g_root[g_table_id], &g_nodes [g_table_id][i], sizeof(struct node));
  g_nodes_index[g_table_id]++;

  if (created) {
     g_last_page_internal_store[g_table_id] = 0;
     g_last_sect_internal_store[g_table_id] = 0;
  } else { 
     g_last_page_internal_store[g_table_id] = mynode_get_last_page_internal_store (g_root[g_table_id]);
     g_last_sect_internal_store[g_table_id] = mynode_get_last_sect_internal_store (g_root[g_table_id]);
     if (mynode_get_internal_block (g_root[g_table_id]) != NODE_INTERNAL_NODE_BLOCK_SIZE[g_table_id]) {
        printf("internal block size in the root is different from the one computed \n"); 
        exit(EXIT_FAILURE);
     }
  }

  return g_root[g_table_id];
}

void* mynode_get_root_nodeAddr () {
  *(page_type*)g_nodeAddr = 0;
  *(section_type*)(g_nodeAddr + SIZE_OF_PAGE) = 0; 
  return g_nodeAddr;
}

void* mynode_get_empty_nodeAddr () {
  *(page_type*)g_nodeAddr = 0;
  *(section_type*)(g_nodeAddr + SIZE_OF_PAGE) = 0; 
  return g_nodeAddr;
}

void* mynode_allocate_addr_for_leaf (int num_page) {
  void* nodeAddr = malloc (NODE_ADDR_SIZE);
  *(page_type*)nodeAddr = num_page;
  *(section_type*)(nodeAddr + SIZE_OF_PAGE) = LEAF_SECTION_DEFAULT_NUMBER; 
  return nodeAddr;
}

void mynode_copy_addr (void* nodeAddrDest, void* nodeAddrSource) {
   memcpy (nodeAddrDest, nodeAddrSource, NODE_ADDR_SIZE);  
}

void* mynode_get_node(void* nodeAddr, int level, bool new_node, bool internal, page_type* num_page_if_new) {
    int cache_index;
    if (new_node) {
        int i = g_nodes_index[g_table_id]++;
        if (internal) {
          bool new_page=false;
          bool recycled;
          if (g_last_page_internal_store[g_table_id] == 0) {
              mynode_get_unused_page_int (level, &recycled);
              g_last_page_internal_store[g_table_id] = *(page_type*)g_nodeAddr;
              g_last_sect_internal_store[g_table_id] = *(section_type*)(g_nodeAddr+SIZE_OF_PAGE);
              if (!recycled) new_page=true;
          }
          g_nodes [g_table_id][i].page = mypager_get_page (g_table_id, g_last_page_internal_store[g_table_id], level, new_page, &cache_index);
          g_nodes [g_table_id][i].pageSection = g_last_sect_internal_store[g_table_id];
          g_nodes [g_table_id][i].nodeSize = NODE_INTERNAL_NODE_BLOCK_SIZE[g_table_id];
          g_nodes [g_table_id][i].nodePtr = g_nodes [g_table_id][i].page + (g_nodes [g_table_id][i].nodeSize * g_nodes [g_table_id][i].pageSection);
          g_nodes [g_table_id][i].cacheIndex = cache_index;
          g_last_sect_internal_store[g_table_id]++;
          *num_page_if_new = g_last_page_internal_store[g_table_id];
          if (g_last_sect_internal_store[g_table_id] >= PAGE_INT_NODE_CAPACITY[g_table_id] || recycled) {
              g_last_sect_internal_store[g_table_id] = 0;
              g_last_page_internal_store[g_table_id] = 0;
          }
        } else {
          *num_page_if_new = mynode_get_unused_page_leaf (level);
          g_nodes [g_table_id][i].page = mypager_get_page (g_table_id, *num_page_if_new, level, new_node, &cache_index);
          g_nodes [g_table_id][i].pageSection = LEAF_SECTION_DEFAULT_NUMBER;
          g_nodes [g_table_id][i].nodeSize = PAGE_SIZE;
          g_nodes [g_table_id][i].nodePtr = g_nodes [g_table_id][i].page;
          g_nodes [g_table_id][i].cacheIndex = cache_index;
        }
        return (void*) &g_nodes [g_table_id][i];
    } else {
       for (int i=0; i<MYDB_MAX_NODES && g_nodes[g_table_id][i].page != NULL; i++) {
          if (mynode_get_node_num_page((void*) &g_nodes [g_table_id][i]) == *(page_type*)nodeAddr && 
                                                                    g_nodes [g_table_id][i].pageSection == *(section_type*)(nodeAddr+SIZE_OF_PAGE)) {
             return (void*) &g_nodes [g_table_id][i];
          }
       }
       int i = g_nodes_index[g_table_id]++;
       g_nodes [g_table_id][i].page = mypager_get_page (g_table_id, *(page_type*)nodeAddr, level, new_node, &cache_index);
       g_nodes [g_table_id][i].pageSection = *(section_type*)(nodeAddr+SIZE_OF_PAGE);
       g_nodes [g_table_id][i].cacheIndex = cache_index;
       if (g_nodes [g_table_id][i].pageSection != LEAF_SECTION_DEFAULT_NUMBER) {
          g_nodes [g_table_id][i].nodeSize = NODE_INTERNAL_NODE_BLOCK_SIZE[g_table_id];
          g_nodes [g_table_id][i].nodePtr = g_nodes [g_table_id][i].page + (g_nodes [g_table_id][i].nodeSize * g_nodes [g_table_id][i].pageSection);
       } else {
          g_nodes [g_table_id][i].nodeSize = PAGE_SIZE;
          g_nodes [g_table_id][i].nodePtr = g_nodes [g_table_id][i].page;
       }
       return (void*) &g_nodes [g_table_id][i];
    }
}

void* mynode_get_node_for_traverse_by_addr(void* nodeAddr, int* index) {
    page_type num_page = *(page_type*)nodeAddr; 
    section_type section = *(section_type*)(nodeAddr+SIZE_OF_PAGE);
    g_node->page = mypager_get_page_for_traverse(num_page, index);
    g_node->pageSection = section;
    if (g_node->pageSection != LEAF_SECTION_DEFAULT_NUMBER) {
        g_node->nodeSize = NODE_INTERNAL_NODE_BLOCK_SIZE[g_table_id];
        g_node->nodePtr = g_node->page + (g_node->nodeSize * g_node->pageSection);
    } else {
        g_node->nodeSize = PAGE_SIZE;
        g_node->nodePtr = g_node->page;
    }
    return (void*) g_node;
}

void* mynode_get_node_by_addr_no_caching(void* nodeAddr, int* index) {
    page_type num_page = *(page_type*)nodeAddr; 
    section_type section = *(section_type*)(nodeAddr+SIZE_OF_PAGE);
    g_node->page = mypager_get_page_no_caching (g_table_id, num_page, index);
    g_node->pageSection = section;
    if (g_node->pageSection != LEAF_SECTION_DEFAULT_NUMBER) {
        g_node->nodeSize = NODE_INTERNAL_NODE_BLOCK_SIZE[g_table_id];
        g_node->nodePtr = g_node->page + (g_node->nodeSize * g_node->pageSection);
    } else {
        g_node->nodeSize = PAGE_SIZE;
        g_node->nodePtr = g_node->page;
    }
    return (void*) g_node;
}

void mynode_free_page (int index) {
    mypager_free_page (g_table_id, index);
}

page_type mynode_get_unused_page_leaf(int level) {
  int index=0;
  if (mynode_get_node_num_pages_dumped_leaf (g_root[g_table_id]) > 0) {
     page_type num_page_unused_node = mynode_get_page_last_leaf_dumped(g_root[g_table_id]);
     *(page_type*)g_nodeAddr = num_page_unused_node;
     *(section_type*)(g_nodeAddr + SIZE_OF_PAGE) = LEAF_SECTION_DEFAULT_NUMBER; 
     void* unusedNode = mynode_get_node_by_addr_no_caching (g_nodeAddr, &index);
     mynode_set_page_last_leaf_dumped (g_root[g_table_id], mynode_get_page_last_leaf_dumped (unusedNode), false);
     mynode_set_node_num_pages_dumped_leaf (g_root[g_table_id], mynode_get_node_num_pages_dumped_leaf (g_root[g_table_id]) - 1, false);
//     printf ("mynode_get_unused_page_leaf, leaf addr %d/%d \n", *(page_type*)g_nodeAddr, LEAF_SECTION_DEFAULT_NUMBER);
     return num_page_unused_node;
  } else {
     return mypager_get_unused_page_num (g_table_id);
  }
}

void mynode_get_unused_page_int(int level, bool* recycled) {
  int index=0;
  if (mynode_get_node_num_pages_dumped_int (g_root[g_table_id]) > 0) {
     *recycled=true;
     void* addr_unused_node = mynode_get_addr_last_int_dumped(g_root[g_table_id]);
     *(page_type*)g_nodeAddr = *(page_type*)addr_unused_node;
     *(section_type*)(g_nodeAddr + SIZE_OF_PAGE) = *(section_type*)(addr_unused_node+SIZE_OF_PAGE); 
     void* unusedNode = mynode_get_node_by_addr_no_caching (addr_unused_node, &index);
     mynode_set_addr_last_int_dumped (g_root[g_table_id], mynode_get_addr_last_int_dumped(unusedNode), false);
     mynode_set_node_num_pages_dumped_int (g_root[g_table_id], mynode_get_node_num_pages_dumped_int (g_root[g_table_id]) - 1, false);
  } else {
     *recycled=false;
     *(page_type*)g_nodeAddr = mypager_get_unused_page_num (g_table_id);
     *(section_type*)(g_nodeAddr + SIZE_OF_PAGE) = 0; 
  }
}

void mynode_dump_page (void* node) {
     if (mynode_is_node_leaf(node)) {
         page_type last_page_dumped = mynode_get_page_last_leaf_dumped(g_root[g_table_id]);
         mynode_set_page_last_leaf_dumped (g_root[g_table_id], mynode_get_node_num_page(node), false);
         mynode_set_node_num_pages_dumped_leaf(g_root[g_table_id], mynode_get_node_num_pages_dumped_leaf(g_root[g_table_id]) + 1, false);
         mynode_erase_page (node);
         mynode_set_page_last_leaf_dumped (node, last_page_dumped, false);
         mynode_set_node_leaf (node, true);
     } else {
         void* last_addr_dumped = mynode_get_addr_last_int_dumped(g_root[g_table_id]);
         *(page_type*)g_nodeAddr1 = *(int*)last_addr_dumped;
         *(section_type*)(g_nodeAddr1 + SIZE_OF_PAGE) = *(section_type*)(last_addr_dumped + SIZE_OF_PAGE); 
         *(page_type*)g_nodeAddr = mynode_get_node_num_page(node);
         *(section_type*)(g_nodeAddr + SIZE_OF_PAGE) = ((struct node *) node)->pageSection; 
         mynode_set_addr_last_int_dumped (g_root[g_table_id], g_nodeAddr, false);
         mynode_set_node_num_pages_dumped_int(g_root[g_table_id], mynode_get_node_num_pages_dumped_int(g_root[g_table_id]) + 1, false);
         mynode_erase_page (node);
         mynode_set_addr_last_int_dumped (node, g_nodeAddr1, false);
         mynode_set_node_leaf (node, false);
     }
}

void mynode_clear_nodes_table () {
   for (int i=0; i<MYDB_MAX_NODES && g_nodes[g_table_id][i].page != NULL; i++) {
      g_nodes[g_table_id][i].page=NULL;
      g_nodes[g_table_id][i].pageSection=0;
   }
   g_nodes_index [g_table_id]= 0;
}

void mynode_erase_page (void* node) {
    memset (((struct node *) node)->nodePtr, '\0', ((struct node *) node)->nodeSize);
}

short mynode_get_max_order(int element_size, int key_size) {
    int page_size = mypager_get_pageSize();
    
    int i, i_internalNode_order, i_leafNode_order, size;

    for (i=2, size=0; i < 5000 && size < page_size; i++) {
        size = NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE + ((NODE_ADDR_SIZE * i) + NODE_ADDR_SIZE)  + (key_size * i);
    };
    i_internalNode_order = i-2;

    for (i=2, size=0; i < 5000 && size < page_size; i++) {
        size = NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE + (element_size * i);
    };
    i_leafNode_order = i-2;

    return (i_internalNode_order <= i_leafNode_order) ? i_internalNode_order : i_leafNode_order;
}

void mynode_save_nodeAddr(void* nodeAddr) {
  *(page_type*)g_nodeAddr = *(page_type*)nodeAddr;
  *(section_type*)(g_nodeAddr + SIZE_OF_PAGE) = *(section_type*)(nodeAddr + SIZE_OF_PAGE);  
}

void* mynode_get_saved_nodeAddr() {
  return g_nodeAddr;
}

short mynode_get_node_order(void* node) {
  return *((short*)( ((struct node *) node)->nodePtr + NODE_ORDER_OFFSET));
}

void mynode_set_node_order(void* node, short order) {
  *((short*)( ((struct node *) node)->nodePtr + NODE_ORDER_OFFSET)) = order;
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

void mynode_set_node_page_size(void* node, int page_size) {
  *((int*)( ((struct node *) node)->nodePtr + NODE_PAGE_SIZE_OFFSET)) = page_size;
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

int mynode_get_node_page_size(void* node) {
  return *((int*)( ((struct node *) node)->nodePtr + NODE_PAGE_SIZE_OFFSET));
}

count_type mynode_get_node_key_count(void* node) {
  return *((count_type*)( ((struct node *) node)->nodePtr + NODE_EL_COUNT_OFFSET));
}

count_type* mynode_get_node_key_count_addr(void* node) {
  return (count_type*)( ((struct node *) node)->nodePtr + NODE_EL_COUNT_OFFSET);
}

void mynode_set_node_key_count(void* node, count_type key_count) {
  *((count_type*)( ((struct node *) node)->nodePtr + NODE_EL_COUNT_OFFSET)) = key_count;
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

bool mynode_is_node_leaf(void* node) {
  short value = *((short*)( ((struct node *) node)->nodePtr + NODE_LEAF_OFFSET));
  return (bool)value;
}

void mynode_set_node_leaf(void* node, bool is_root) {
 short value = is_root;
  *((short*)( ((struct node *) node)->nodePtr + NODE_LEAF_OFFSET)) = value;
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

count_type mynode_get_node_pages_count(void* node) {
  return *((count_type*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_OFFSET));
}

void mynode_set_node_pages_count(void* node, count_type pages_count) {
  *((count_type*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_OFFSET)) = pages_count;
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

page_type mynode_get_node_num_page(void* node) {
  return *((page_type*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGE_OFFSET));
}

void mynode_set_node_num_page(void* node, page_type num_page, bool defrag) {
  *((page_type*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGE_OFFSET)) = num_page;
  if (!defrag) {
      mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
  }
}

void* mynode_get_addr_last_int_dumped(void* node) {
  return (void*) (((struct node *) node)->nodePtr + NODE_LAST_PAGE_INT_DUMPED_OFFSET);
}

void mynode_set_addr_last_int_dumped(void* node, void* nodeAddr, bool defrag) {
  memcpy( (((struct node *) node)->nodePtr + NODE_LAST_PAGE_INT_DUMPED_OFFSET), nodeAddr, SIZE_OF_PAGE + SIZE_OF_SECTION);
  if (!defrag) {
      mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
  }
}

page_type mynode_get_page_last_leaf_dumped(void* node) {
  return *((page_type*)( ((struct node *) node)->nodePtr + NODE_LAST_PAGE_LEAF_DUMPED_OFFSET));
}

void mynode_set_page_last_leaf_dumped(void* node, page_type last_page_dumped, bool defrag) {
  *((page_type*)( ((struct node *) node)->nodePtr + NODE_LAST_PAGE_LEAF_DUMPED_OFFSET)) = last_page_dumped;
  if (!defrag) {
      mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
  }
}

page_type mynode_get_node_num_pages_dumped_int(void* node) {
  return *((page_type*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_DUMPED_INT_OFFSET));
}

void mynode_set_node_num_pages_dumped_int(void* node, count_type num_pages_dumped, bool defrag) {
  *((count_type*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_DUMPED_INT_OFFSET)) = num_pages_dumped;
  if (!defrag) {
      mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
  }
}

count_type mynode_get_node_num_pages_dumped_leaf(void* node) {
  return *((count_type*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_DUMPED_LEAF_OFFSET));
}

void mynode_set_node_num_pages_dumped_leaf(void* node, count_type num_pages_dumped, bool defrag) {
  *((count_type*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_DUMPED_LEAF_OFFSET)) = num_pages_dumped;
  if (!defrag) {
     mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
  }
}

page_type mynode_get_node_link_page(void* node) {
  return *(page_type*) (((struct node *) node)->nodePtr + NODE_LINK_PAGE_OFFSET);
}

void* mynode_get_node_link(void* node) {
  return (void*) (((struct node *) node)->nodePtr + NODE_LINK_PAGE_OFFSET);
}

void mynode_set_node_link(void* node, void* address) {
  memcpy(mynode_get_node_link (node), address, NODE_ADDR_SIZE);
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

void mynode_set_node_link_page(void* node, page_type page, bool defrag) {
  *(page_type*)mynode_get_node_link(node) = page;
  if (!defrag) {
     mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
  }
}

void mynode_set_node_link_with_nodeAddr(void* nodeDest, void* nodeSource) {
  *(page_type*)g_nodeAddr2 = mynode_get_node_num_page (nodeSource);
  *(section_type*)(g_nodeAddr2 + SIZE_OF_PAGE) = ((struct node *) nodeSource)->pageSection;  
  memcpy(mynode_get_node_link (nodeDest), g_nodeAddr2, NODE_ADDR_SIZE);
}

void mynode_init_node_link(void* node) {
  *(page_type*)g_nodeAddr2 = 0;
  *(section_type*)(g_nodeAddr2 + SIZE_OF_PAGE) = 0;  
  memcpy(mynode_get_node_link (node), g_nodeAddr2, NODE_ADDR_SIZE);
}

void* mynode_get_node_el(void* node, count_type key_num) {
  if (mynode_is_node_leaf(node)) {
     if (mynode_get_node_num_page(node) == 0) {
        return ((struct node *) node)->nodePtr + NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET + key_num * NODE_ELEMENT_SIZE[g_table_id];
     } else {
        return ((struct node *) node)->nodePtr + NODE_LEAF_ELEMENT_BLOCK_OFFSET + key_num * NODE_ELEMENT_SIZE[g_table_id];
     }
  } else {
     if (mynode_get_node_num_page(node) == 0) {
        return ((struct node *) node)->nodePtr + NODE_ROOT_INTERNAL_ELEMENT_BLOCK_OFFSET[g_table_id] + key_num * NODE_KEY_SIZE[g_table_id];
     } else {
        return ((struct node *) node)->nodePtr + NODE_INTERNAL_ELEMENT_BLOCK_OFFSET[g_table_id] + key_num * NODE_KEY_SIZE[g_table_id];
     }
  }
}

void* mynode_set_node_el(void* node, count_type ind, void* el) {
  memcpy(mynode_get_node_el (node, ind), el, NODE_ELEMENT_SIZE[g_table_id]);
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

void* mynode_set_node_elKey(void* node, count_type ind, void* elKey) {
  memcpy(mynode_get_node_el (node, ind), elKey, NODE_KEY_SIZE[g_table_id]);
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

void* mynode_erase_node_el(void* node, count_type ind) {
  memset (mynode_get_node_el(node, ind), '\0', NODE_ELEMENT_SIZE[g_table_id]);
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

void* mynode_erase_node_elKey(void* node, count_type ind) {
  memset (mynode_get_node_el(node, ind), '\0', NODE_KEY_SIZE[g_table_id]);
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

void* mynode_set_node_child_from_node(void* nodeDest, count_type ind, void* nodeSource) {
  *(page_type*)g_nodeAddr = mynode_get_node_num_page(nodeSource);
  *(section_type*)(g_nodeAddr + SIZE_OF_PAGE) = ((struct node *) nodeSource)->pageSection; 

  if (mynode_get_node_num_page(nodeDest) == 0) {
     memcpy( (((struct node *) nodeDest)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind), g_nodeAddr, NODE_ADDR_SIZE);
  } else {
     memcpy( (((struct node *) nodeDest)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind), g_nodeAddr, NODE_ADDR_SIZE);
  };
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) nodeDest)->cacheIndex );
}

page_type mynode_get_node_child_page(void* node, count_type ind) {
  if (mynode_get_node_num_page(node) == 0) {
     return *((page_type*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind));
  } else {
     return *((page_type*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind));
  }
}

void* mynode_get_node_child(void* node, count_type ind) {
  if (mynode_get_node_num_page(node) == 0) {
     return ((void*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind));
  } else {
     return ((void*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind));
  }
}

void mynode_set_node_child(void* node, count_type ind, void* childAddr) {
  if (mynode_get_node_num_page(node) == 0) {
     if (childAddr) {
        *((page_type*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind)) = *(page_type*)childAddr;
        *((section_type*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + SIZE_OF_PAGE + NODE_ADDR_SIZE * ind)) = 
                                                                                                    *(section_type*)(childAddr+SIZE_OF_PAGE);
     } else {
        *((page_type*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind)) = 0;
        *((section_type*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + SIZE_OF_PAGE + NODE_ADDR_SIZE * ind)) = 0;
     }
  } else {
     if (childAddr) {
        *((page_type*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind)) = *(page_type*)childAddr;
        *((section_type*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + SIZE_OF_PAGE + NODE_ADDR_SIZE * ind)) =
                                                                                                            *(section_type*)(childAddr+SIZE_OF_PAGE);
     } else {
        *((page_type*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind)) = 0;
        *((section_type*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + SIZE_OF_PAGE + NODE_ADDR_SIZE * ind)) = 0;
     }
  }
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}


void mynode_set_node_child_page(void* node, count_type ind, page_type num_page, bool defrag) {
  if (mynode_get_node_num_page(node) == 0) {
     *((page_type*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind)) = num_page;
  } else {
     *((page_type*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind)) = num_page;
  }
  if (!defrag) {
      mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
  }
}

page_type mynode_get_page_from_address(void* nodeAddr) {
  return *(page_type*) nodeAddr;
}

section_type mynode_get_section_from_address(void* nodeAddr) {
  return *(section_type*) (nodeAddr+SIZE_OF_PAGE);
}

section_type mynode_get_last_sect_internal_store(void* node) {
  return *((section_type*)( ((struct node *) node)->nodePtr + NODE_LAST_SECT_INTERNAL_STORE_OFFSET));
}

void mynode_set_last_sect_internal_store(void* node, section_type sect) {
  *((section_type*)( ((struct node *) node)->nodePtr + NODE_LAST_SECT_INTERNAL_STORE_OFFSET)) = sect;
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

page_type mynode_get_last_page_internal_store(void* node) {
  return *((page_type*)( ((struct node *) node)->nodePtr + NODE_LAST_PAGE_INTERNAL_STORE_OFFSET));
}

void mynode_set_last_page_internal_store(void* node, page_type page) {
  *((page_type*)( ((struct node *) node)->nodePtr + NODE_LAST_PAGE_INTERNAL_STORE_OFFSET)) = page;
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

int mynode_get_internal_block(void* node) {
  return *((int*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_BLOCK_SIZE_OFFSET));    
}

void mynode_set_internal_block(void* node, int blockSize) {
  *((int*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_BLOCK_SIZE_OFFSET)) = blockSize;
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

void mynode_mark_asToBeFlushed(void* node) {
  mypager_mark_asToBeFlushed(g_table_id, ((struct node *) node)->cacheIndex );
}

void mynode_setNode (void* node, count_type key_count, bool leaf, count_type pages_count, page_type num_page, bool root) {
  memset (((struct node *) node)->nodePtr, '\0', ((struct node *) node)->nodeSize);
  mynode_set_node_leaf(node, leaf);
  mynode_set_node_num_page(node, num_page, false);
  mynode_set_node_key_count(node, key_count);
  mynode_set_page_last_leaf_dumped (node, 0, false);
  mynode_set_addr_last_int_dumped (node, mynode_get_empty_nodeAddr(), false);
  if (root) {
     mynode_set_node_order(node, TREE_ORDER[g_table_id]);
     mynode_set_node_page_size(node, PAGE_SIZE);
     mynode_set_node_pages_count(node, pages_count);
     mynode_set_node_num_pages_dumped_int(node, 0, false);
     mynode_set_node_num_pages_dumped_leaf(node, 0, false);
     mynode_set_last_page_internal_store (node, 0);
     mynode_set_last_sect_internal_store (node, 0);
     mynode_set_internal_block (node, NODE_INTERNAL_NODE_BLOCK_SIZE[g_table_id]);
   }
   mynode_init_node_link (node);
}

void mynode_resetRoot (void* node, count_type key_count, bool leaf, count_type pages_count, page_type num_page) {
   count_type num_pages_dumped_int = mynode_get_node_num_pages_dumped_int(node);
   count_type num_pages_dumped_leaf = mynode_get_node_num_pages_dumped_leaf(node);
   page_type page_last_leaf_dumped = mynode_get_page_last_leaf_dumped(node);
   page_type last_page_internal_store = mynode_get_last_page_internal_store(node);
   section_type last_sect_internal_store = mynode_get_last_sect_internal_store(node);
   *(page_type*)g_nodeAddr1 = *(page_type*)mynode_get_addr_last_int_dumped(node);
   *(section_type*)(g_nodeAddr1 + SIZE_OF_PAGE) = *(section_type*)(mynode_get_addr_last_int_dumped(node) + SIZE_OF_PAGE); 
   short orderTree = mynode_get_node_order (node);
   int pageSize = mynode_get_node_page_size (node);

   memset (((struct node *) node)->nodePtr, '\0', ((struct node *) node)->nodeSize);

   mynode_set_node_leaf(node, leaf); 
   mynode_set_node_num_page(node, num_page, false);
   mynode_set_node_key_count(node, key_count);
   mynode_set_page_last_leaf_dumped (node, page_last_leaf_dumped, false);
   mynode_set_addr_last_int_dumped (node, g_nodeAddr1, false);
   mynode_set_node_order(node, orderTree);
   mynode_set_node_page_size (node, pageSize);
   mynode_set_node_pages_count(node, pages_count);
   mynode_set_node_num_pages_dumped_int(node, num_pages_dumped_int, false);
   mynode_set_node_num_pages_dumped_leaf(node, num_pages_dumped_leaf, false);
   mynode_set_last_page_internal_store (node, last_page_internal_store);
   mynode_set_last_sect_internal_store (node, last_sect_internal_store);
   mynode_set_internal_block (node, NODE_INTERNAL_NODE_BLOCK_SIZE[g_table_id]);
   mynode_init_node_link (node);
}

void mynode_convert_from_root (void* nodeDest, void* nodeSource, page_type num_page_dest) {
  struct node* pageDestS = (struct node *) nodeDest;
  struct node* pageSourceS = (struct node *) nodeSource;

  mynode_set_node_leaf(nodeDest, mynode_is_node_leaf(nodeSource));
  mynode_set_node_num_page(nodeDest, num_page_dest, false);
  mynode_set_node_key_count(nodeDest, mynode_get_node_key_count(nodeSource));
  mynode_set_node_link (nodeDest, mynode_get_node_link (nodeSource));

  if (mynode_is_node_leaf(nodeSource)) { 
    memcpy (pageDestS->nodePtr + NODE_LEAF_ELEMENT_BLOCK_OFFSET, pageSourceS->nodePtr + NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET, 
                                                                            PAGE_SIZE - NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET);
  } else {
    memcpy (pageDestS->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET, 
            pageSourceS->nodePtr + NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE, 
            (NODE_ADDR_SIZE * (TREE_ORDER[g_table_id] + 1)) + TREE_ORDER[g_table_id] * NODE_KEY_SIZE[g_table_id]);
  }    
}

void mynode_convert_to_root (void* nodeDest, void* nodeSource) {
  struct node* pageDestS = (struct node *) nodeDest;
  struct node* pageSourceS = (struct node *) nodeSource;

  mynode_set_node_leaf(nodeDest, mynode_is_node_leaf(nodeSource));
  mynode_set_node_key_count(nodeDest, mynode_get_node_key_count(nodeSource));

  if (mynode_is_node_leaf(nodeSource)) {
      memcpy (pageDestS->nodePtr + NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE, 
              pageSourceS->nodePtr + NODE_LEAF_ELEMENT_BLOCK_OFFSET, 
              TREE_ORDER[g_table_id] * NODE_ELEMENT_SIZE[g_table_id]);
  } else {
      memcpy (pageDestS->nodePtr + NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE, 
              pageSourceS->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET, 
              (NODE_ADDR_SIZE * (TREE_ORDER[g_table_id] + 1)) + (TREE_ORDER[g_table_id] * NODE_KEY_SIZE[g_table_id]));
  };
}

void mynode_update_root_pages_count (void* node) {
    mynode_set_node_pages_count (node, mypager_get_num_pages(g_table_id));
}

void mynode_check () {
}

int mynode_get_page_size () {
    return mypager_get_pageSize();
}

