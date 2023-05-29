#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "node.h"

static void* mybptreeS_search_node (int table_id, void* nodeAddr, void* key, int level);
static void* mybptreeS_positioning_cursor (int table_id, void* lower_key, void* upper_key, short key_size, page_type* page_num, count_type* pos_el);
static void* mybptreeS_node_positioning_cursor (int table_id, void* nodeAddr, void* lower_key, void* upper_key, int level, page_type* page_num, count_type* pos_el);

#define MAX_TABLES 10

static bool (*g_keyEl1_greater_keyEl2[MAX_TABLES])(void*, void*);
static bool (*g_keyEl1_equal_keyEl2[MAX_TABLES])(void*, void*);
static bool (*g_key1_greater_keyEl2[MAX_TABLES])(void*, void*);
static bool (*g_key1_equal_keyEl2[MAX_TABLES])(void*, void*);

struct cursorSearch {
  void* lower_key;
  void* upper_key;
  void* element;
  int element_size;
  page_type page_num;
  count_type pos_el;
  bool end_cursor;
};
#define MAX_CURSORSSearch 20
static struct cursorSearch* g_cursorsSearch [MAX_TABLES][MAX_CURSORSSearch];

struct cursorErase {
  void* lower_key;
  void* upper_key;
  void* current_key;
  int key_size;
  void* element;
  int element_size;
  bool end_cursor;
};
#define MAX_CURSORSErase 20
static struct cursorErase* g_cursorsErase [MAX_TABLES][MAX_CURSORSErase];

static void* g_node_just_searched_in_search_key;

void mybptreeS_setup () {
}

void* mybptreeS_search_key (int table_id, void* key, short key_size) {
//    printf ("mybptreeS_search_key, key %d/%d \n", *(int*)key, *(int*)(key+4));
    mynode_set_table_id (table_id);    
    void* element = mybptreeS_search_node (table_id, mynode_get_root_nodeAddr(), key, 1);
    mynode_flush_cache_operation();
    return element;
}

void* mybptreeS_search_node (int table_id, void* nodeAddr, void* key, int level) {
    page_type num_page_if_new;
    void* node = mynode_get_node(nodeAddr, level, false, false, &num_page_if_new);

    count_type n = mynode_get_node_key_count (node); 
    count_type i;
    bool leaf = mynode_is_node_leaf(node);
    bool found = false;

    if (leaf) {
        for (i=0; i<n && ( g_key1_equal_keyEl2[table_id] (key, mynode_get_node_el(node, i)) || 
                                                            g_key1_greater_keyEl2[table_id] (key, mynode_get_node_el(node, i)) ); i++) {
            if (g_key1_equal_keyEl2[table_id] (key, mynode_get_node_el(node, i))) {
                found = true;
                break;
            }
        }
    } else {
        for (i=0; i<n && ( g_key1_equal_keyEl2[table_id] (key, mynode_get_node_el(node, i)) ||
                                                             g_key1_greater_keyEl2[table_id] (key, mynode_get_node_el(node, i)) ); i++);
    }

    if (leaf && found) {
        g_node_just_searched_in_search_key = node;
        return mynode_get_node_el(node, i);
    } else if (leaf && !found) {
        return NULL;
    } else {
        level++;
        return mybptreeS_search_node (table_id, mynode_get_node_child(node, i), key, level);
    }
}

int mybptreeS_open_cursor (int table_id, void* lowerKey, void* upperKey, int key_size, int element_size) {
    mynode_set_table_id (table_id);    
    
    int i;
    for (i=0; i<MAX_CURSORSSearch && g_cursorsSearch[table_id][i] != NULL; i++);
    g_cursorsSearch[table_id][i] = (struct cursorSearch*)malloc(sizeof(struct cursorSearch));

    void* lKey = (void*) malloc (key_size);
    void* uKey = (void*) malloc (key_size);
    void* element = (void*) malloc (element_size);
    memcpy (lKey, lowerKey, key_size);
    memcpy (uKey, upperKey, key_size);
    g_cursorsSearch[table_id][i]->lower_key = lKey;
    g_cursorsSearch[table_id][i]->upper_key = uKey;
    g_cursorsSearch[table_id][i]->element=element;
    g_cursorsSearch[table_id][i]->element_size=element_size;
    g_cursorsSearch[table_id][i]->end_cursor=false;

    page_type page_num;
    count_type pos_el;
    void* el = mybptreeS_positioning_cursor (table_id, lKey, uKey, key_size, &page_num, &pos_el);
    g_cursorsSearch[table_id][i]->page_num = page_num;
    g_cursorsSearch[table_id][i]->pos_el = pos_el;

    return ++i;
}

void* mybptreeS_fetch_cursor (int table_id, int cursor_id) {
    mynode_set_table_id (table_id);    

    cursor_id--;
    page_type num_page_if_new=0;
    void* nodeAddr = mynode_allocate_addr_for_leaf (g_cursorsSearch[table_id][cursor_id]->page_num);
    void* node = mynode_get_node(nodeAddr, 0, false, false, &num_page_if_new);

    void* element = mynode_get_node_el(node, g_cursorsSearch[table_id][cursor_id]->pos_el);

    if (g_key1_greater_keyEl2[table_id] (element, g_cursorsSearch[table_id][cursor_id]->upper_key)) {
        g_cursorsSearch[table_id][cursor_id]->end_cursor=true;
        free (nodeAddr);
        nodeAddr=NULL;
        return NULL;
    }

    if (g_key1_greater_keyEl2[table_id] (g_cursorsSearch[table_id][cursor_id]->lower_key, element)) {
        g_cursorsSearch[table_id][cursor_id]->end_cursor=true;
        free (nodeAddr);
        nodeAddr=NULL;
        return NULL;
    }

    g_cursorsSearch[table_id][cursor_id]->pos_el++;
    if (!(g_cursorsSearch[table_id][cursor_id]->pos_el < mynode_get_node_key_count(node))) {
        if (mynode_get_node_link (node) > 0) {
            g_cursorsSearch[table_id][cursor_id]->page_num = mynode_get_page_from_address(mynode_get_node_link (node));
            g_cursorsSearch[table_id][cursor_id]->pos_el = 0;
        } else {
            g_cursorsSearch[table_id][cursor_id]->end_cursor=true;
            free (nodeAddr);
            nodeAddr=NULL;
            return NULL;
        }
    }
    
    memcpy (g_cursorsSearch[table_id][cursor_id]->element, element, g_cursorsSearch[table_id][cursor_id]->element_size);    
    mynode_flush_cache_operation();
    free (nodeAddr);
    nodeAddr=NULL;
    return g_cursorsSearch[table_id][cursor_id]->element;
}

void mybptreeS_close_cursor (int table_id, int cursor_id) {
    mynode_set_table_id (table_id);    

    cursor_id--;
    free (g_cursorsSearch[table_id][cursor_id]->lower_key);
    g_cursorsSearch[table_id][cursor_id]->lower_key = NULL;
    free (g_cursorsSearch[table_id][cursor_id]->upper_key);
    g_cursorsSearch[table_id][cursor_id]->upper_key = NULL;
    free (g_cursorsSearch[table_id][cursor_id]->element);
    g_cursorsSearch[table_id][cursor_id]->element = NULL;
    free (g_cursorsSearch[table_id][cursor_id]);
    g_cursorsSearch[table_id][cursor_id] = NULL;
}


int mybptreeS_open_cursor_xErase (int table_id, void* lowerKey, void* upperKey, int key_size, int element_size) {
    mynode_set_table_id (table_id);    

    int i;
    for (i=0; i<MAX_CURSORSErase && g_cursorsErase[table_id][i] != NULL; i++);
    g_cursorsErase[table_id][i] = (struct cursorErase*)malloc(sizeof(struct cursorErase));

    void* lKey = (void*) malloc (key_size);
    void* uKey = (void*) malloc (key_size);
    void* element = (void*) malloc (element_size);
    memcpy (lKey, lowerKey, key_size);
    memcpy (uKey, upperKey, key_size);
    g_cursorsErase[table_id][i]->lower_key = lKey;
    g_cursorsErase[table_id][i]->upper_key = uKey;
    g_cursorsErase[table_id][i]->element=element;
    g_cursorsErase[table_id][i]->element_size=element_size;
    g_cursorsErase[table_id][i]->end_cursor=false;
    g_cursorsErase[table_id][i]->key_size = key_size;

    int page_num, pos_el;
    void* el = mybptreeS_positioning_cursor (table_id, lKey, uKey, key_size, &page_num, &pos_el);
    void* current_key = (void*) malloc (key_size);
    memcpy (current_key, el, key_size);
    g_cursorsErase[table_id][i]->current_key = current_key;

    return ++i;
}

void* mybptreeS_fetch_cursor_xErase (int table_id, int cursor_id) {
    mynode_set_table_id (table_id);    

    cursor_id--;
    if (g_key1_greater_keyEl2[table_id] (g_cursorsErase[table_id][cursor_id]->current_key, g_cursorsErase[table_id][cursor_id]->upper_key)) {
        mynode_flush_cache_operation();
        g_cursorsErase[table_id][cursor_id]->end_cursor=true;
        return NULL;
    }

    if (g_key1_greater_keyEl2[table_id] (g_cursorsErase[table_id][cursor_id]->lower_key, g_cursorsErase[table_id][cursor_id]->current_key)) {
        mynode_flush_cache_operation();
        g_cursorsErase[table_id][cursor_id]->end_cursor=true;
        return NULL;
    }

    int i;
    void* element = mybptreeS_search_key (table_id, g_cursorsErase[table_id][cursor_id]->current_key, g_cursorsErase[table_id][cursor_id]->key_size);
    count_type n = mynode_get_node_key_count (g_node_just_searched_in_search_key); 
    for (i=0; i<n && !( g_key1_equal_keyEl2[table_id] (g_cursorsErase[table_id][cursor_id]->current_key, 
                                                    mynode_get_node_el(g_node_just_searched_in_search_key, i)) ); i++);

    if (i+1 < n) {
        memcpy (g_cursorsErase[table_id][cursor_id]->current_key, mynode_get_node_el(g_node_just_searched_in_search_key, i+1), 
                                                                                                                    g_cursorsErase[table_id][cursor_id]->key_size);
        memcpy (g_cursorsErase[table_id][cursor_id]->element, element, g_cursorsErase[table_id][cursor_id]->element_size);
        mynode_flush_cache_operation();
        return g_cursorsErase[table_id][cursor_id]->element;
    } else if (mynode_get_node_link (g_node_just_searched_in_search_key) > 0) {
        int num_page_if_new = 0;
        void* next_node = mynode_get_node(mynode_get_node_link (g_node_just_searched_in_search_key), 0, false, false, &num_page_if_new);
        memcpy (g_cursorsErase[table_id][cursor_id]->current_key, mynode_get_node_el(next_node, 0), g_cursorsErase[table_id][cursor_id]->key_size);        
        memcpy (g_cursorsErase[table_id][cursor_id]->element, element, g_cursorsErase[table_id][cursor_id]->element_size);
        mynode_flush_cache_operation();
        return g_cursorsErase[table_id][cursor_id]->element;
    } else {
        g_cursorsErase[table_id][cursor_id]->end_cursor=true;
        mynode_flush_cache_operation();
        return NULL;
    }
    
}

void mybptreeS_close_cursor_xErase (int table_id, int cursor_id) {
    mynode_set_table_id (table_id);    

    cursor_id--;
    free (g_cursorsErase[table_id][cursor_id]->lower_key);
    g_cursorsErase[table_id][cursor_id]->lower_key = NULL;
    free (g_cursorsErase[table_id][cursor_id]->upper_key);
    g_cursorsErase[table_id][cursor_id]->upper_key = NULL;
    free (g_cursorsErase[table_id][cursor_id]->current_key);
    g_cursorsErase[table_id][cursor_id]->current_key = NULL;
    free (g_cursorsErase[table_id][cursor_id]->element);
    g_cursorsErase[table_id][cursor_id]->element = NULL;
    free (g_cursorsErase[table_id][cursor_id]);
    g_cursorsErase[table_id][cursor_id] = NULL;
}

void* mybptreeS_positioning_cursor (int table_id, void* lower_key, void* upper_key, short key_size, page_type* page_num, count_type* pos_el) {
    void* element = mybptreeS_node_positioning_cursor (table_id, mynode_get_root_nodeAddr(), lower_key, upper_key, 1, page_num, pos_el);
    return element;
}

void* mybptreeS_node_positioning_cursor (int table_id, void* nodeAddr, void* lower_key, void* upper_key, int level, page_type* page_num, count_type* pos_el) {
//    printf ("mybptreeS_node_positioning_cursor, addr %d/%d \n", *(int*)nodeAddr, *(short*)(nodeAddr+4));
    page_type num_page_if_new;
    void* node = mynode_get_node(nodeAddr, level, false, false, &num_page_if_new);

    count_type n = mynode_get_node_key_count (node); 
    count_type i;
    bool leaf = mynode_is_node_leaf(node);

    for (i=0; i<n && ( g_key1_equal_keyEl2[table_id] (lower_key, mynode_get_node_el(node, i)) || 
                                        g_key1_greater_keyEl2[table_id] (lower_key, mynode_get_node_el(node, i)) ); i++);

    if (leaf) {
        void* nodeAddrCurr = mynode_allocate_addr_for_leaf (0);
        mynode_copy_addr (nodeAddrCurr, nodeAddr);
        while (true) {
            if (i <n) {
                if (  ( g_key1_greater_keyEl2[table_id] (mynode_get_node_el(node, i), lower_key) || 
                                        g_key1_equal_keyEl2[table_id] (mynode_get_node_el(node, i), lower_key) ) &&
                      ( g_key1_greater_keyEl2[table_id] (upper_key, mynode_get_node_el(node, i)) || 
                                        g_key1_equal_keyEl2[table_id] (mynode_get_node_el(node, i), upper_key) )   ) {
                    *page_num = mynode_get_page_from_address(nodeAddrCurr);
                    *pos_el = i;
                    free (nodeAddrCurr);
                    nodeAddrCurr = NULL;
                    return mynode_get_node_el(node, i);
                } else if (g_key1_greater_keyEl2[table_id] (mynode_get_node_el(node, i), upper_key)) {
                    *page_num = mynode_get_page_from_address(nodeAddrCurr);
                    *pos_el = i;
                    free (nodeAddrCurr);
                    nodeAddrCurr = NULL;
                    return mynode_get_node_el(node, i);
                } else {
                    i++;
                }
            } else {
                void* linkAddr = mynode_get_node_link (node);
                if (mynode_get_page_from_address(linkAddr) > 0) {
                    mynode_copy_addr (nodeAddrCurr, linkAddr);
                    node = mynode_get_node(nodeAddrCurr, 0, false, false, &num_page_if_new);
                    n = mynode_get_node_key_count (node);
                    i=0;
                } else {
                    *page_num = mynode_get_page_from_address(nodeAddrCurr);
                    *pos_el = i;
                    free (nodeAddrCurr);
                    nodeAddrCurr = NULL;
                    return mynode_get_node_el(node, i);
                }
            }
        }
    } else {
        level++;
        return mybptreeS_node_positioning_cursor (table_id, mynode_get_node_child(node, i), lower_key, upper_key, level, page_num, pos_el);
    }
}

int mybptreeS_update_element (int table_id, void* key, short key_size, void* element, short element_size) {
    mynode_set_table_id (table_id);    

    void* el = mybptreeS_search_node (table_id, mynode_get_root_nodeAddr(), key, 1);
    if (!el) return -1;

    count_type n = mynode_get_node_key_count (g_node_just_searched_in_search_key); 
    int i;
    for (i=0; i<n && !( g_key1_equal_keyEl2 [table_id](key, mynode_get_node_el(g_node_just_searched_in_search_key, i)) ); i++);

    mynode_set_node_el (g_node_just_searched_in_search_key, i, element);
    return 0;
}

void mybptreeS_set_fn_key_test (int table_id,
                              bool (*keyEl1_greater_keyEl2)(void*, void*), 
                              bool (*keyEl1_equal_keyEl2  )(void*, void*),
                              bool (*key1_greater_keyEl2  )(void*, void*),
                              bool (*key1_equal_keyEl2    )(void*, void*)) {
    g_keyEl1_greater_keyEl2[table_id] = keyEl1_greater_keyEl2;
    g_keyEl1_equal_keyEl2[table_id] = keyEl1_equal_keyEl2;
    g_key1_greater_keyEl2[table_id] = key1_greater_keyEl2;
    g_key1_equal_keyEl2[table_id] = key1_equal_keyEl2;
}

