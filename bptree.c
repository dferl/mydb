#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "node.h"
#include "bptreeS.h"
#include "bptreeIE.h"

#define MAX_TABLES 10

static int NODE_ELEMENT_SIZE[MAX_TABLES];
static short TREE_ORDER[MAX_TABLES];
static short NODE_KEY_SIZE[MAX_TABLES];
static count_type NODE_MIN[MAX_TABLES];
static void* g_root[MAX_TABLES];
static bool g_connected[MAX_TABLES]={false, false, false, false, false, false, false, false, false, false};
static bool g_functionSetup[MAX_TABLES]={false, false, false, false, false, false, false, false, false, false};
static bool g_readOnly[MAX_TABLES]={false, false, false, false, false, false, false, false, false, false};

int mybptree_openBTree(const char* filename, int element_size, short order, short key_size, bool readOnly) { 
    int table_id = mynode_open_session (element_size, order, key_size, filename, readOnly);
    mynode_set_table_id (table_id);    

    g_connected[table_id] = true;
    g_readOnly[table_id] = readOnly;
    NODE_KEY_SIZE[table_id] = key_size;
    NODE_ELEMENT_SIZE[table_id] = element_size;
    TREE_ORDER[table_id] = order;
    if ( (order%2) == 0) {
       NODE_MIN[table_id] = (order/2) - 1;
    } else {
       NODE_MIN[table_id] = order/2;
    }

    bool created;
    g_root[table_id] = mynode_get_root (&created);
    if (created) {
        mynode_setNode (g_root[table_id], 0, true, 1, 0, true);
    }

    if (TREE_ORDER[table_id] != mynode_get_node_order(g_root[table_id])) {
        printf("the order parameter is different from the order in the DB root %d, %d \n", TREE_ORDER[table_id], 
                                                                                                    mynode_get_node_order(g_root[table_id]));
        exit(EXIT_FAILURE);
    }

    if (mynode_get_page_size() != mynode_get_node_page_size(g_root[table_id])) {
        printf("the page size parameter is different from the page size in the DB root %d, %d \n", mynode_get_page_size(), 
                                                                                                    mynode_get_node_page_size(g_root[table_id]));
        exit(EXIT_FAILURE);
    }

    mybptreeIE_setup (table_id, element_size, NODE_MIN[table_id], g_root[table_id]);
    mybptreeS_setup ();
    return table_id;
}

void mybptree_closeBTree(int table_id) {
    g_connected[table_id] = false;
    mynode_set_table_id (table_id);    
    mynode_close_session ();
}

short mybptree_get_max_order(int element_size, int key_size) {
    return mynode_get_max_order(element_size, key_size);
}

void mybptree_set_fn_key_test (int table_id,
                              bool (*keyEl1_greater_keyEl2)(void*, void*), 
                              bool (*keyEl1_equal_keyEl2  )(void*, void*),
                              bool (*key1_greater_keyEl2  )(void*, void*),
                              bool (*key1_equal_keyEl2    )(void*, void*)) {
    g_functionSetup[table_id] = true;
    mybptreeIE_set_fn_key_test (table_id, keyEl1_greater_keyEl2, keyEl1_equal_keyEl2, key1_greater_keyEl2, key1_equal_keyEl2);
    mybptreeS_set_fn_key_test (table_id, keyEl1_greater_keyEl2, keyEl1_equal_keyEl2, key1_greater_keyEl2, key1_equal_keyEl2);
}

int mybptree_insertBTree (int table_id, void* element) {
    if (!g_connected[table_id]) {
      printf("There is no connection; insert operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    if (!g_functionSetup[table_id]) {
      printf("Key test functions have note been set; insert operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    if (g_readOnly[table_id]) {
        printf("Insert is not allowed in a read only connection \n");
        exit(EXIT_FAILURE);
    }

    return mybptreeIE_insertBTree (table_id, element);
}

int mybptree_eraseBTree(int table_id, void* key, short key_size) {
    if (!g_connected[table_id]) {
      printf("There is no connection; erase operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    if (!g_functionSetup[table_id]) {
      printf("Key test functions have note been set; erase operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    if (g_readOnly[table_id]) {
        printf("Erase is not allowed in a read only connection \n");
        exit(EXIT_FAILURE);
    }
    return mybptreeIE_eraseBTree(table_id, key, key_size);
}

void* mybptree_search_key (int table_id, void* key, short key_size) {
    if (!g_connected[table_id]) {
      printf("There is no connection; search operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    return mybptreeS_search_key (table_id, key, key_size);
}

int mybptree_open_cursor (int table_id, void* lowerKey, void* upperKey, int key_size, int element_size) {
    if (!g_connected[table_id]) {
      printf("There is no connection; open cursor operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    if (!g_functionSetup[table_id]) {
      printf("Key test functions have note been set; open cursor operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    if (!g_readOnly[table_id]) {
        printf("Open read only cursor is not allowed in a not read only connection \n");
        exit(EXIT_FAILURE);
    }
    return mybptreeS_open_cursor (table_id, lowerKey, upperKey, key_size, element_size);
}

void* mybptree_fetch_cursor (int table_id, int cursor_id) {
    if (cursor_id==0) {
      printf("Cursor not open; fetch operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    return mybptreeS_fetch_cursor (table_id, cursor_id);
}

void mybptree_close_cursor (int table_id, int cursor_id) {
    if (cursor_id==0) {
      printf("Cursor not open; close operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    mybptreeS_close_cursor (table_id, cursor_id);
}

int mybptree_open_cursor_xErase (int table_id, void* lowerKey, void* upperKey, int key_size, int element_size) {
    if (!g_connected[table_id]) {
      printf("There is no connection; open cursor operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    if (!g_functionSetup[table_id]) {
      printf("Key test functions have note been set; open cursor operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    if (g_readOnly[table_id]) {
        printf("Open cursor x Erase is not allowed in a read only connection \n");
        exit(EXIT_FAILURE);
    }
    return mybptreeS_open_cursor_xErase (table_id, lowerKey, upperKey, key_size, element_size);
}

void* mybptree_fetch_cursor_xErase (int table_id, int cursor_id) {
    if (cursor_id==0) {
      printf("Cursor not open; fetch operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    return mybptreeS_fetch_cursor_xErase (table_id, cursor_id);
}

void mybptree_close_cursor_xErase (int table_id, int cursor_id) {
    if (cursor_id==0) {
      printf("Cursor not open; close operation is not allowed\n");
      exit(EXIT_FAILURE);
    }
    mybptreeS_close_cursor_xErase (table_id, cursor_id);
}

int mybptree_update_element (int table_id, void* key, short key_size, void* element, short element_size) {
    if (g_readOnly[table_id]) {
        printf("Update is not allowed in a read only connection \n");
        exit(EXIT_FAILURE);
    }
    return mybptreeS_update_element (table_id, key, key_size, element, element_size);
}

