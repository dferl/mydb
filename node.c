#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "pager.h"

/*
 * Common Node Block Layout
 */
#define NODE_COMMON_BLOCK_OFFSET 0
#define NODE_LEAF_OFFSET 0
#define NODE_LEAF_SIZE sizeof(short)
#define NODE_NUM_PAGE_OFFSET NODE_LEAF_OFFSET + NODE_LEAF_SIZE
#define NODE_NUM_PAGE_SIZE sizeof(int)
#define NODE_EL_COUNT_OFFSET NODE_NUM_PAGE_OFFSET + NODE_NUM_PAGE_SIZE
#define NODE_EL_COUNT_SIZE sizeof(short)
#define NODE_LAST_PAGE_INT_DUMPED_OFFSET NODE_EL_COUNT_OFFSET + NODE_EL_COUNT_SIZE
#define NODE_LAST_PAGE_INT_DUMPED_SIZE sizeof(int)
#define NODE_LAST_SECT_INT_DUMPED_OFFSET NODE_LAST_PAGE_INT_DUMPED_OFFSET + NODE_LAST_PAGE_INT_DUMPED_SIZE
#define NODE_LAST_SECT_INT_DUMPED_SIZE sizeof(short)
#define NODE_LAST_PAGE_LEAF_DUMPED_OFFSET NODE_LAST_SECT_INT_DUMPED_OFFSET + NODE_LAST_SECT_INT_DUMPED_SIZE
#define NODE_LAST_PAGE_LEAF_DUMPED_SIZE sizeof(int)
#define NODE_COMMON_BLOCK_SIZE NODE_LEAF_SIZE + \
                               NODE_NUM_PAGE_SIZE + \
                               NODE_EL_COUNT_SIZE + \
                               NODE_LAST_PAGE_INT_DUMPED_SIZE + \
                               NODE_LAST_SECT_INT_DUMPED_SIZE + \
                               NODE_LAST_PAGE_LEAF_DUMPED_SIZE \

/*
 * Root Node Block Layout 
 */
#define NODE_ROOT_BLOCK_OFFSET NODE_COMMON_BLOCK_SIZE
#define NODE_ORDER_OFFSET NODE_INTERNAL_BLOCK_OFFSET
#define NODE_ORDER_SIZE sizeof(short)
#define NODE_NUM_PAGES_DUMPED_INT_OFFSET NODE_ORDER_OFFSET + NODE_ORDER_SIZE
#define NODE_NUM_PAGES_DUMPED_INT_SIZE sizeof(int)
#define NODE_NUM_PAGES_DUMPED_LEAF_OFFSET NODE_NUM_PAGES_DUMPED_INT_OFFSET + NODE_NUM_PAGES_DUMPED_INT_SIZE
#define NODE_NUM_PAGES_DUMPED_LEAF_SIZE sizeof(int)
#define NODE_NUM_PAGES_OFFSET NODE_NUM_PAGES_DUMPED_LEAF_OFFSET + NODE_NUM_PAGES_DUMPED_LEAF_SIZE
#define NODE_NUM_PAGES_SIZE sizeof(int)
#define NODE_ROOT_LINK_OFFSET NODE_NUM_PAGES_OFFSET + NODE_NUM_PAGES_SIZE
#define NODE_ROOT_LINK_SIZE sizeof(int)
#define NODE_LAST_PAGE_INTERNAL_STORE_OFFSET NODE_ROOT_LINK_OFFSET + NODE_ROOT_LINK_SIZE
#define NODE_LAST_PAGE_INTERNAL_STORE_SIZE sizeof(int)
#define NODE_LAST_SECT_INTERNAL_STORE_OFFSET NODE_LAST_PAGE_INTERNAL_STORE_OFFSET + NODE_LAST_PAGE_INTERNAL_STORE_SIZE
#define NODE_LAST_SECT_INTERNAL_STORE_SIZE sizeof(short)
#define NODE_ROOT_BLOCK_SIZE NODE_ORDER_SIZE + \
                                 NODE_NUM_PAGES_DUMPED_INT_SIZE + \
                                 NODE_NUM_PAGES_DUMPED_LEAF_SIZE + \
                                 NODE_NUM_PAGES_SIZE + \
                                 NODE_ROOT_LINK_SIZE + \
                                 NODE_LAST_PAGE_INTERNAL_STORE_SIZE + \
                                 NODE_LAST_SECT_INTERNAL_STORE_SIZE
#define NODE_ROOT_CHILDREN_BLOCK_OFFSET NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE
static int NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET;
static int NODE_ROOT_INTERNAL_ELEMENT_BLOCK_OFFSET;

/*
 * Internal Node Block Layout
 */
#define NODE_INTERNAL_BLOCK_OFFSET NODE_COMMON_BLOCK_SIZE
#define NODE_INTERNAL_BLOCK_SIZE 0
#define NODE_INTERNAL_CHILDREN_BLOCK_OFFSET NODE_COMMON_BLOCK_SIZE + NODE_INTERNAL_BLOCK_SIZE
static int NODE_INTERNAL_ELEMENT_BLOCK_OFFSET;
static int NODE_INTERNAL_NODE_BLOCK_SIZE;

/*
 * Leaf Node Block Layout
 */
#define NODE_LEAF_BLOCK_OFFSET NODE_COMMON_BLOCK_SIZE
#define NODE_LEAF_LINK_OFFSET NODE_LEAF_BLOCK_OFFSET
#define NODE_LEAF_LINK_SIZE sizeof(int)
#define NODE_LEAF_BLOCK_SIZE NODE_LEAF_LINK_SIZE
#define NODE_LEAF_ELEMENT_BLOCK_OFFSET NODE_LEAF_BLOCK_OFFSET + NODE_LEAF_BLOCK_SIZE

#define NODE_ADDR_SIZE (sizeof(int) + sizeof(short))

/*
 * Root/Internal/Leaf Node Variable parts Layout
 */
static int PAGE_SIZE;
static int NODE_ELEMENT_SIZE;
static int NODE_KEY_SIZE;
static int NODE_ELEMENTS_BLOCK_SIZE;
static int NODE_KEYS_BLOCK_SIZE;
static int NODE_CHILDREN_BLOCK_SIZE;
static short TREE_ORDER;
static short PAGE_INT_NODE_CAPACITY;
static short SIZE_OF_INTEGER;

struct node {
  int  nodeSize;
  short pageSection;
  void* nodePtr;
  void* page;
};

void mynode_setNode (void* node, short key_count, bool leaf, int pages_count, int num_page, bool root);
short mynode_get_node_order(void* node);
void mynode_set_node_order(void* node, short order);
short mynode_get_node_key_count(void* node);
short* mynode_get_node_key_count_addr(void* node);
void mynode_set_node_key_count(void* node, short key_count);
short mynode_get_node_min(void* node);
void mynode_set_node_min(void* node, short min);
bool mynode_is_node_leaf(void* node);
void mynode_set_node_leaf(void* node, bool is_root);
int mynode_get_node_pages_count(void* node);
void mynode_set_node_pages_count(void* node, int pages_count);
int mynode_get_node_num_page(void* node);
void mynode_set_node_num_page(void* node, int num_page);
void* mynode_get_node_el(void* node, int key_num);
void* mynode_get_node_child(void* node, short ind);
int mynode_get_node_child_page(void* node, short ind);
void mynode_set_node_child(void* node, short ind, void* childAddr);
void* mynode_set_node_el(void* node, short ind, void* el);
void* mynode_set_node_elKey(void* node, short ind, void* elKey);
void* mynode_erase_node_el(void* node, short ind);
void* mynode_erase_node_elKey(void* node, short ind);
void mynode_open_session(int element_size, short order, int key_size, const char* filename);
short mynode_get_max_order(int element_size, int key_size);
void mynode_convert_to_root (void* nodeDest, void* nodeSource);
void mynode_convert_from_root (void* nodeDest, void* nodeSource, int num_page_dest);
void* mynode_get_root (bool* created);
void mynode_flush_cache_operation ();
void* mynode_get_node(void* nodeAddr, int level, bool new_page, bool internal, int* num_page_if_new);
void mynode_update_root_pages_count (void* node);
void mynode_free_page (int index);
void mynode_erase_page (void* node);
void* mynode_get_root_nodeAddr ();
int mynode_get_page_from_address(void* nodeAddr);
short mynode_get_section_from_address(void* nodeAddr);
void mynode_save_nodeAddr(void* nodeAddr);
void* mynode_get_saved_nodeAddr();
void mynode_dump_page (void* node);

static int mynode_get_node_num_pages_dumped_int(void* node);
static void mynode_set_node_num_pages_dumped_int(void* node, int num_pages_dumped);
static int mynode_get_node_num_pages_dumped_leaf(void* node);
static void mynode_set_node_num_pages_dumped_leaf(void* node, int num_pages_dumped);

static void* mynode_get_addr_last_int_dumped (void* node);
static void mynode_set_addr_last_int_dumped (void* node, void* nodeAddr);
static int mynode_get_page_last_leaf_dumped(void* node);
static void mynode_set_page_last_leaf_dumped(void* node, int last_page_dumped);

static void mynode_set_last_sect_internal_store(void* node, short page);
static int mynode_get_last_sect_internal_store(void* node);
static void mynode_set_last_page_internal_store(void* node, int page);
static int mynode_get_last_page_internal_store(void* node);
static int mynode_get_unused_page_leaf (int level);
static void mynode_get_unused_page_int (int level);
static void mynode_clear_nodes_table ();
static void* mynode_get_empty_nodeAddr();

void* g_root;
static int g_nodes_index;
static int g_last_page_internal_store;
static short g_last_sect_internal_store;
static void* g_nodeAddr;

#define MYDB_MAX_NODES 10000
struct node g_nodes [MYDB_MAX_NODES];

void mynode_open_session(int element_size, short order, int key_size, const char* filename) {
  mypager_open(filename, order);

  SIZE_OF_INTEGER = sizeof(int);
  NODE_ELEMENT_SIZE = element_size;
  TREE_ORDER = order;
  PAGE_SIZE = mypager_get_pageSize();
  NODE_KEY_SIZE = key_size;
  NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET = NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE;
  NODE_ROOT_INTERNAL_ELEMENT_BLOCK_OFFSET = NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE + (NODE_ADDR_SIZE * (TREE_ORDER + 1));
  NODE_INTERNAL_ELEMENT_BLOCK_OFFSET = NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + (NODE_ADDR_SIZE * (TREE_ORDER + 1));

  if (NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET + TREE_ORDER * NODE_ELEMENT_SIZE > PAGE_SIZE) {
      printf("too many elements per leaf page; page size %d, tree_order %d, element size %d, leaf block %d \n", PAGE_SIZE, TREE_ORDER, NODE_ELEMENT_SIZE, 
                                                                                                                 NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET);
      exit(EXIT_FAILURE);
  }

  if (NODE_ROOT_INTERNAL_ELEMENT_BLOCK_OFFSET + TREE_ORDER * NODE_KEY_SIZE > PAGE_SIZE) {
      printf("too many elements per internal page; page size %d, tree_order %d, key size %d, num page size %ld, internal block %d \n", 
                                                                                                                PAGE_SIZE,
                                                                                                                TREE_ORDER,
                                                                                                                NODE_ELEMENT_SIZE,
                                                                                                                NODE_ADDR_SIZE,
                                                                                                                NODE_ROOT_INTERNAL_ELEMENT_BLOCK_OFFSET);
      exit(EXIT_FAILURE);
  }

  NODE_INTERNAL_NODE_BLOCK_SIZE = NODE_INTERNAL_ELEMENT_BLOCK_OFFSET +  TREE_ORDER * NODE_KEY_SIZE;

  PAGE_INT_NODE_CAPACITY = PAGE_SIZE / NODE_INTERNAL_NODE_BLOCK_SIZE;
  if (PAGE_INT_NODE_CAPACITY > 9999) PAGE_INT_NODE_CAPACITY = 9998;

  g_nodes_index = 0;

  for (int i=0; i<MYDB_MAX_NODES; i++) g_nodes[i].page=NULL;

  g_nodeAddr = malloc (sizeof (int) + sizeof(short));
  g_root = malloc (sizeof (struct node));
}

void mynode_close_session() {
  mynode_set_last_page_internal_store (g_root, g_last_page_internal_store);
  mynode_set_last_sect_internal_store (g_root, g_last_sect_internal_store);
  mypager_flush_free_cache ();
  mypager_close();
  free (g_nodeAddr);
  g_nodeAddr=NULL;
}

void mynode_flush_cache_operation () {
  mypager_flush_cache_operation ();
  mynode_clear_nodes_table();
}

void* mynode_get_root (bool* created) {
  if (g_pager->num_pages == 0) {
      *created = true;
      g_nodes [g_nodes_index].page = (void*) mypager_get_page(0, 0, true);
  } else {
      *created = false;
      g_nodes [g_nodes_index].page = (void *) mypager_get_page(0, 0, false);
  }
  
  g_nodes [g_nodes_index].pageSection = 9999;
  g_nodes [g_nodes_index].nodeSize = PAGE_SIZE;
  g_nodes [g_nodes_index].nodePtr = g_nodes [g_nodes_index].page;
  memcpy (g_root,  &g_nodes [g_nodes_index], sizeof(struct node));
  g_nodes_index++;

  if (created) {
     g_last_page_internal_store = mynode_get_last_page_internal_store (g_root);
     g_last_sect_internal_store = mynode_get_last_sect_internal_store (g_root);
  } else { 
     g_last_page_internal_store = 0;
     g_last_sect_internal_store = 0;
  }

  return g_root;
}

void* mynode_get_root_nodeAddr () {
  *(int*)g_nodeAddr = 0;
  *(short*)(g_nodeAddr + SIZE_OF_INTEGER) = 0; 
  return g_nodeAddr;
}

void* mynode_get_empty_nodeAddr () {
  *(int*)g_nodeAddr = 0;
  *(short*)(g_nodeAddr + SIZE_OF_INTEGER) = 0; 
  return g_nodeAddr;
}

void* mynode_get_node(void* nodeAddr, int level, bool new_page, bool internal, int* num_page_if_new) {
    if (new_page) {
        int i = g_nodes_index++;
        if (internal) {
          if (g_last_page_internal_store == 0) {
              mynode_get_unused_page_int (level);
              g_last_page_internal_store = *(int*)g_nodeAddr;
              g_last_sect_internal_store = *(short*)(g_nodeAddr+sizeof(short));
          }
          g_nodes [i].page = mypager_get_page (g_last_page_internal_store, level, new_page);
          g_nodes [i].pageSection = g_last_sect_internal_store++;
          g_nodes [i].nodeSize = NODE_INTERNAL_NODE_BLOCK_SIZE;
          g_nodes [i].nodePtr = g_nodes [i].page + (g_nodes [i].nodeSize * g_nodes [i].pageSection);
          *num_page_if_new = g_last_page_internal_store;
          if (g_last_sect_internal_store >= PAGE_INT_NODE_CAPACITY) {
              g_last_sect_internal_store = 0;
              g_last_page_internal_store = 0;
          }
        } else {
          *num_page_if_new = mynode_get_unused_page_leaf (level);
          g_nodes [i].page = mypager_get_page (*num_page_if_new, level, new_page);
          g_nodes [i].pageSection = 9999;
          g_nodes [i].nodeSize = PAGE_SIZE;
          g_nodes [i].nodePtr = g_nodes [i].page;
        }
        return (void*) &g_nodes [i];
    } else {
       for (int i=0; i<MYDB_MAX_NODES && g_nodes[i].page != NULL; i++) {
          if (mynode_get_node_num_page((void*) &g_nodes [i]) == *(int*)nodeAddr && g_nodes [i].pageSection == *(short*)(nodeAddr+SIZE_OF_INTEGER)) {
             return (void*) &g_nodes [i];
          }
       }
       int i = g_nodes_index++;
       g_nodes [i].page = mypager_get_page (*(int*)nodeAddr, level, new_page);
       g_nodes [i].pageSection = *(short*)(nodeAddr+SIZE_OF_INTEGER);
       if (g_nodes [i].pageSection != 9999) {
          g_nodes [i].nodeSize = NODE_INTERNAL_NODE_BLOCK_SIZE;
          g_nodes [i].nodePtr = g_nodes [i].page + (g_nodes [i].nodeSize * g_nodes [i].pageSection);
       } else {
          g_nodes [i].nodeSize = PAGE_SIZE;
          g_nodes [i].nodePtr = g_nodes [i].page;
       }
       return (void*) &g_nodes [i];
    }
}

void* mynode_get_node_for_traverse_by_addr(void* nodeAddr, int* index) {
    int num_page = *(int*)nodeAddr; 
    int section = *(short*)(nodeAddr+sizeof(int));
    g_nodes [0].page = mypager_get_page_for_traverse(num_page, index);
    g_nodes [0].pageSection = section;
    if (g_nodes [0].pageSection != 9999) {
        g_nodes [0].nodeSize = NODE_INTERNAL_NODE_BLOCK_SIZE;
        g_nodes [0].nodePtr = g_nodes [0].page + (g_nodes [0].nodeSize * g_nodes [0].pageSection);
    } else {
        g_nodes [0].nodeSize = PAGE_SIZE;
        g_nodes [0].nodePtr = g_nodes [0].page;
    }
    return (void*) &g_nodes [0];
}

void* mynode_get_node_for_traverse_by_page(int num_page, int* index) {
    g_nodes [0].page = mypager_get_page_for_traverse(num_page, index);
    g_nodes [0].pageSection = 9999;
    g_nodes [0].nodeSize = PAGE_SIZE;
    g_nodes [0].nodePtr = g_nodes [0].page;
    return (void*) &g_nodes [0];
}

void mynode_free_page (int index) {
    mypager_free_page (index);
}

int mynode_get_unused_page_leaf(int level) {
  int num_page=0;
  if (mynode_get_node_num_pages_dumped_leaf (g_root) > 0) {
     int num_page_unused_node = mynode_get_page_last_leaf_dumped(g_root);
     *(int*)g_nodeAddr = num_page_unused_node;
     *(short*)(g_nodeAddr + SIZE_OF_INTEGER) = 0; 
     void* unusedNode = mynode_get_node(g_nodeAddr, level, false, false, &num_page);
     mynode_set_page_last_leaf_dumped (g_root, mynode_get_page_last_leaf_dumped (unusedNode));
     mynode_set_node_num_pages_dumped_leaf (g_root, mynode_get_node_num_pages_dumped_leaf (g_root) - 1);
     return num_page_unused_node;
  } else {
     return mypager_get_unused_page_num ();
  }
}

void mynode_get_unused_page_int(int level) {
  int num_page=0;
  if (mynode_get_node_num_pages_dumped_int (g_root) > 0) {
     void* addr_unused_node = mynode_get_addr_last_int_dumped(g_root);
     void* unusedNode = mynode_get_node(addr_unused_node, level, false, false, &num_page);
     mynode_set_addr_last_int_dumped (g_root, mynode_get_addr_last_int_dumped(unusedNode));
     mynode_set_node_num_pages_dumped_int (g_root, mynode_get_node_num_pages_dumped_int (g_root) - 1);
     *(int*)g_nodeAddr = *(int*)addr_unused_node;
     *(short*)(g_nodeAddr + SIZE_OF_INTEGER) = *(short*)(addr_unused_node+sizeof(int)); 
  } else {
     *(int*)g_nodeAddr = mypager_get_unused_page_num ();
     *(short*)(g_nodeAddr + SIZE_OF_INTEGER) = 0; 
  }
}

void mynode_dump_page (void* node) {
     if (mynode_is_node_leaf(node)) {
         int last_page_dumped = mynode_get_page_last_leaf_dumped(g_root);
         mynode_set_page_last_leaf_dumped (g_root, mynode_get_node_num_page(node));
         mynode_set_node_num_pages_dumped_leaf(g_root, mynode_get_node_num_pages_dumped_leaf(g_root) + 1);
         mynode_erase_page (node);
         mynode_set_page_last_leaf_dumped (node, last_page_dumped);
     } else {
         void* last_addr_dumped = mynode_get_addr_last_int_dumped(g_root);
         *(int*)g_nodeAddr = mynode_get_node_num_page(node);
         *(short*)(g_nodeAddr + SIZE_OF_INTEGER) = ((struct node *) node)->pageSection; 
         mynode_set_addr_last_int_dumped (g_root, g_nodeAddr);
         mynode_set_node_num_pages_dumped_int(g_root, mynode_get_node_num_pages_dumped_int(g_root) + 1);
         mynode_erase_page (node);
         mynode_set_addr_last_int_dumped (node, last_addr_dumped);
     }
}

void mynode_clear_nodes_table () {
   for (int i=0; i<MYDB_MAX_NODES && g_nodes[i].page != NULL; i++) {
      g_nodes[i].page=NULL;
      g_nodes[i].pageSection=0;
   }
   g_nodes_index = 0;
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
  *(int*)g_nodeAddr = *(int*)nodeAddr;
  *(short*)(g_nodeAddr + SIZE_OF_INTEGER) = *(short*)(nodeAddr + SIZE_OF_INTEGER);  
}

void* mynode_get_saved_nodeAddr() {
  return g_nodeAddr;
}

short mynode_get_node_order(void* node) {
  return *((short*)( ((struct node *) node)->nodePtr + NODE_ORDER_OFFSET));
}

void mynode_set_node_order(void* node, short order) {
  *((short*)( ((struct node *) node)->nodePtr + NODE_ORDER_OFFSET)) = order;
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

short mynode_get_node_key_count(void* node) {
  return *((short*)( ((struct node *) node)->nodePtr + NODE_EL_COUNT_OFFSET));
}

short* mynode_get_node_key_count_addr(void* node) {
  return (short*)( ((struct node *) node)->nodePtr + NODE_EL_COUNT_OFFSET);
}

void mynode_set_node_key_count(void* node, short key_count) {
  *((short*)( ((struct node *) node)->nodePtr + NODE_EL_COUNT_OFFSET)) = key_count;
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

bool mynode_is_node_leaf(void* node) {
  short value = *((short*)( ((struct node *) node)->nodePtr + NODE_LEAF_OFFSET));
  return (bool)value;
}

void mynode_set_node_leaf(void* node, bool is_root) {
 short value = is_root;
  *((short*)( ((struct node *) node)->nodePtr + NODE_LEAF_OFFSET)) = value;
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

int mynode_get_node_pages_count(void* node) {
  return *((int*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_OFFSET));
}

void mynode_set_node_pages_count(void* node, int pages_count) {
  *((int*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_OFFSET)) = pages_count;
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

int mynode_get_node_num_page(void* node) {
  return *((int*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGE_OFFSET));
}

void mynode_set_node_num_page(void* node, int num_page) {
  *((int*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGE_OFFSET)) = num_page;
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

void* mynode_get_addr_last_int_dumped(void* node) {
  return (void*) (((struct node *) node)->nodePtr + NODE_LAST_PAGE_INT_DUMPED_OFFSET);
}

void mynode_set_addr_last_int_dumped(void* node, void* nodeAddr) {
  memcpy( (((struct node *) node)->nodePtr + NODE_LAST_PAGE_INT_DUMPED_OFFSET), nodeAddr, SIZE_OF_INTEGER + sizeof(short));
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

int mynode_get_page_last_leaf_dumped(void* node) {
  return *((int*)( ((struct node *) node)->nodePtr + NODE_LAST_PAGE_LEAF_DUMPED_OFFSET));
}

void mynode_set_page_last_leaf_dumped(void* node, int last_page_dumped) {
  *((int*)( ((struct node *) node)->nodePtr + NODE_LAST_PAGE_LEAF_DUMPED_OFFSET)) = last_page_dumped;
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

int mynode_get_node_num_pages_dumped_int(void* node) {
  return *((int*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_DUMPED_INT_OFFSET));
}

void mynode_set_node_num_pages_dumped_int(void* node, int num_pages_dumped) {
  *((int*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_DUMPED_INT_OFFSET)) = num_pages_dumped;
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

int mynode_get_node_num_pages_dumped_leaf(void* node) {
  return *((int*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_DUMPED_LEAF_OFFSET));
}

void mynode_set_node_num_pages_dumped_leaf(void* node, int num_pages_dumped) {
  *((int*)( ((struct node *) node)->nodePtr + NODE_NUM_PAGES_DUMPED_LEAF_OFFSET)) = num_pages_dumped;
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

int mynode_get_node_leaf_link(void* node) {
  if (mynode_get_node_num_page (node) == 0) {
    return *((int*)( ((struct node *) node)->nodePtr + NODE_ROOT_LINK_OFFSET));
  } else {
    return *((int*)( ((struct node *) node)->nodePtr + NODE_LEAF_LINK_OFFSET));
  }
}

void mynode_set_node_leaf_link(void* node, int leaf_link) {
  if (mynode_get_node_num_page (node) == 0) {
    *((int*)( ((struct node *) node)->nodePtr + NODE_ROOT_LINK_OFFSET)) = leaf_link;
  } else {
    *((int*)( ((struct node *) node)->nodePtr + NODE_LEAF_LINK_OFFSET)) = leaf_link;
  }
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

void* mynode_get_node_el(void* node, int key_num) {
  if (mynode_is_node_leaf(node)) {
     if (mynode_get_node_num_page(node) == 0) {
        return ((struct node *) node)->nodePtr + NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET + key_num * NODE_ELEMENT_SIZE;
     } else {
        return ((struct node *) node)->nodePtr + NODE_LEAF_ELEMENT_BLOCK_OFFSET + key_num * NODE_ELEMENT_SIZE;
     }
  } else {
     if (mynode_get_node_num_page(node) == 0) {
        return ((struct node *) node)->nodePtr + NODE_ROOT_INTERNAL_ELEMENT_BLOCK_OFFSET + key_num * NODE_KEY_SIZE;
     } else {
        return ((struct node *) node)->nodePtr + NODE_INTERNAL_ELEMENT_BLOCK_OFFSET + key_num * NODE_KEY_SIZE;
     }
  }
}

void* mynode_set_node_el(void* node, short ind, void* el) {
  memcpy(mynode_get_node_el (node, ind), el, NODE_ELEMENT_SIZE);
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

void* mynode_set_node_elKey(void* node, short ind, void* elKey) {
  memcpy(mynode_get_node_el (node, ind), elKey, NODE_KEY_SIZE);
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

void* mynode_erase_node_el(void* node, short ind) {
  memset (mynode_get_node_el(node, ind), '\0', NODE_ELEMENT_SIZE);
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

void* mynode_erase_node_elKey(void* node, short ind) {
  memset (mynode_get_node_el(node, ind), '\0', NODE_KEY_SIZE);
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

extern void* mynode_set_node_child_from_node(void* nodeDest, short ind, void* nodeSource) {
  *(int*)g_nodeAddr = mynode_get_node_num_page(nodeSource);
  *(short*)(g_nodeAddr + SIZE_OF_INTEGER) = ((struct node *) nodeSource)->pageSection; 

  if (mynode_get_node_num_page(nodeDest) == 0) {
     memcpy( (((struct node *) nodeDest)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind), g_nodeAddr, SIZE_OF_INTEGER + sizeof(short));
  } else {
     memcpy( (((struct node *) nodeDest)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind), g_nodeAddr, SIZE_OF_INTEGER + sizeof(short));
  };
  mypager_mark_asToBeFlushed( ((struct node *) nodeDest)->page );
}

int mynode_get_node_child_page(void* node, short ind) {
  if (mynode_get_node_num_page(node) == 0) {
     return *((int*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind));
  } else {
     return *((int*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind));
  }
}

void* mynode_get_node_child(void* node, short ind) {
  if (mynode_get_node_num_page(node) == 0) {
     return ((void*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind));
  } else {
     return ((void*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind));
  }
}

void mynode_set_node_child(void* node, short ind, void* childAddr) {
  if (mynode_get_node_num_page(node) == 0) {
     if (childAddr) {
        *((int*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind)) = *(int*)childAddr;
        *((short*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + SIZE_OF_INTEGER + NODE_ADDR_SIZE * ind)) = 
                                                                                                    *(short*)(childAddr+SIZE_OF_INTEGER);
     } else {
        *((int*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind)) = 0;
        *((short*)( ((struct node *) node)->nodePtr + NODE_ROOT_CHILDREN_BLOCK_OFFSET + SIZE_OF_INTEGER + NODE_ADDR_SIZE * ind)) = 0;
     }
  } else {
     if (childAddr) {
        *((int*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind)) = *(int*)childAddr;
        *((short*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + SIZE_OF_INTEGER + NODE_ADDR_SIZE * ind)) =
                                                                                                            *(short*)(childAddr+SIZE_OF_INTEGER);
     } else {
        *((int*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + NODE_ADDR_SIZE * ind)) = 0;
        *((short*)( ((struct node *) node)->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET + SIZE_OF_INTEGER + NODE_ADDR_SIZE * ind)) = 0;
     }
  }
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

int mynode_get_page_from_address(void* nodeAddr) {
  return *(int*) nodeAddr;
}

short mynode_get_section_from_address(void* nodeAddr) {
  return *(short*) (nodeAddr+sizeof(int));
}

int mynode_get_last_sect_internal_store(void* node) {
  return *((int*)( ((struct node *) node)->nodePtr + NODE_LAST_SECT_INTERNAL_STORE_OFFSET));
}

void mynode_set_last_sect_internal_store(void* node, short sect) {
  *((short*)( ((struct node *) node)->nodePtr + NODE_LAST_SECT_INTERNAL_STORE_OFFSET)) = sect;
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

int mynode_get_last_page_internal_store(void* node) {
  return *((int*)( ((struct node *) node)->nodePtr + NODE_LAST_PAGE_INTERNAL_STORE_OFFSET));
}

void mynode_set_last_page_internal_store(void* node, int page) {
  *((int*)( ((struct node *) node)->nodePtr + NODE_LAST_PAGE_INTERNAL_STORE_OFFSET)) = page;
  mypager_mark_asToBeFlushed( ((struct node *) node)->page );
}

void mynode_setNode (void* node, short key_count, bool leaf, int pages_count, int num_page, bool root) {
  memset (((struct node *) node)->nodePtr, '\0', ((struct node *) node)->nodeSize);

  mynode_set_node_leaf(node, leaf);
  mynode_set_node_num_page(node, num_page);
  mynode_set_node_key_count(node, key_count);
  if (root) {
     mynode_set_node_order(node, TREE_ORDER);
     mynode_set_node_pages_count(node, pages_count);
     mynode_set_node_num_pages_dumped_int(node, 0);
     mynode_set_node_num_pages_dumped_leaf(node, 0);
     mynode_set_page_last_leaf_dumped (node, 0);
     mynode_set_addr_last_int_dumped (node, mynode_get_empty_nodeAddr);
     mynode_set_last_page_internal_store (node, 0);
     mynode_set_last_sect_internal_store (node, 0);
   }
   if (leaf && !root) {
     mynode_set_node_leaf_link (node, 0);
   }
}

void mynode_convert_from_root (void* nodeDest, void* nodeSource, int num_page_dest) {
  struct node* pageDestS = (struct node *) nodeDest;
  struct node* pageSourceS = (struct node *) nodeSource;

  mynode_set_node_leaf(nodeDest, mynode_is_node_leaf(nodeSource));
  mynode_set_node_num_page(nodeDest, num_page_dest);
  mynode_set_node_key_count(nodeDest, mynode_get_node_key_count(nodeSource));
 
  if (mynode_is_node_leaf(nodeSource)) { 
    mynode_set_node_leaf_link (nodeDest, mynode_get_node_leaf_link (nodeSource));
    memcpy (pageDestS->nodePtr + NODE_LEAF_ELEMENT_BLOCK_OFFSET, pageSourceS->nodePtr + NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET, 
                                                                            PAGE_SIZE - NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET);
  } else {
    memcpy (pageDestS->nodePtr + NODE_INTERNAL_CHILDREN_BLOCK_OFFSET, 
            pageSourceS->nodePtr + NODE_COMMON_BLOCK_SIZE + NODE_ROOT_BLOCK_SIZE, 
            (NODE_ADDR_SIZE * (TREE_ORDER + 1)) + TREE_ORDER * NODE_KEY_SIZE);
  }    
}

void mynode_convert_to_root (void* nodeDest, void* nodeSource) {
  struct node* pageDestS = (struct node *) nodeDest;
  struct node* pageSourceS = (struct node *) nodeSource;

  mynode_set_node_leaf(nodeDest, mynode_is_node_leaf(nodeSource));
  mynode_set_node_num_page(nodeDest, mynode_get_node_num_page(nodeSource));
  mynode_set_node_key_count(nodeDest, mynode_get_node_key_count(nodeSource));

  memcpy (pageDestS->nodePtr + NODE_ROOT_LEAF_ELEMENT_BLOCK_OFFSET, 
          pageSourceS->nodePtr + NODE_LEAF_ELEMENT_BLOCK_OFFSET, 
          PAGE_SIZE - NODE_LEAF_ELEMENT_BLOCK_OFFSET);

  mynode_set_node_order(nodeDest, TREE_ORDER);
  mynode_set_node_pages_count(nodeDest, 0);
  mynode_set_node_num_pages_dumped_int(nodeDest, 0);
  mynode_set_node_num_pages_dumped_leaf(nodeDest, 0);
  mynode_set_page_last_leaf_dumped (nodeDest, 0);
  mynode_set_addr_last_int_dumped (nodeDest, mynode_get_empty_nodeAddr);
}

void mynode_update_root_pages_count (void* node) {
    mynode_set_node_pages_count (node, g_pager->num_pages);
}

