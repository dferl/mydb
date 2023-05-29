#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "node.h"

static void* mybptreeIE_insertNode (int table_id, void * node, int level);
static void mybptreeIE_setNode (void* node, count_type key_count, bool leaf, count_type pages_count, page_type num_page, bool root);
static void mybptreeIE_addNewTreeLevel (int table_id, void* right, int level);
static void* mybptreeIE_split (int table_id, void* node, int level);
static short mybptreeIE_shiftDataRightReplEl (void* node, count_type i, count_type* size, void* newEl);
static short mybptreeIE_shiftDataLeft (void* node, count_type i, count_type* size);
static int mybptreeIE_shiftNodeRight (void* node, count_type i, count_type size);
static int mybptreeIE_shiftNodeLeft (void* node, count_type i, count_type size);
static int mybptreeIE_eraseNode(int table_id, void* node, void* key, int level, void* parent, count_type iParent);
static void mybptreeIE_dump_page (void* node);
static void mybptreeIE_borrow_internal (void* node, count_type base, bool toLeft);
static void mybptreeIE_borrow_leaf (void* node, count_type base, bool toLeft);
static void mybptreeIE_mergeChildren (void* node, count_type leftIdx);
static void* mybptreeIE_getSmallestValueSubTree (void* node, count_type idx);

static page_type g_num_page_if_new;
static int g_resInsert;
static void* g_upshiftElement;
static void* g_elementToInsert;
static void* g_keyToCancel;

#define MAX_TABLES 10

static bool (*g_keyEl1_greater_keyEl2[MAX_TABLES])(void*, void*);
static bool (*g_keyEl1_equal_keyEl2[MAX_TABLES])(void*, void*);
static bool (*g_key1_greater_keyEl2[MAX_TABLES])(void*, void*);
static bool (*g_key1_equal_keyEl2[MAX_TABLES])(void*, void*);

static void* g_root[MAX_TABLES];
static int NODE_ELEMENT_SIZE[MAX_TABLES];
static count_type NODE_MIN[MAX_TABLES];

void mybptreeIE_setup (int table_id, int element_size, int node_min, void* root) {
    NODE_ELEMENT_SIZE[table_id] = element_size;
    NODE_MIN[table_id] = node_min;
    g_root[table_id] = root;
}

int mybptreeIE_insertBTree (int table_id, void* element) {
//    printf ("mybptreeIE_insertBTree, key %d/%d \n", *(int*)element, *(short*)(element+4));
    mynode_set_table_id (table_id);    

    g_upshiftElement = (void*) malloc(NODE_ELEMENT_SIZE[table_id]);
    g_elementToInsert = (void*) malloc(NODE_ELEMENT_SIZE[table_id]);

    g_resInsert = 1;

    memcpy(g_elementToInsert, element, NODE_ELEMENT_SIZE[table_id]);
    mybptreeIE_insertNode (table_id, g_root[table_id], 0);
    mynode_flush_cache_operation ();
    
    free (g_upshiftElement);
    free (g_elementToInsert);
    g_upshiftElement = NULL;
    g_elementToInsert = NULL;

    return g_resInsert;
}

void* mybptreeIE_insertNode (int table_id, void * node, int level) {
//    printf ("mybptreeIE_insertNode, node %d/%d \n", mynode_get_node_num_page(node), ((struct node*)node)->pageSection);

    level++;

    count_type i = 0;
    while (i< mynode_get_node_key_count(node) && g_keyEl1_greater_keyEl2[table_id] (g_elementToInsert, mynode_get_node_el(node, i)) ) {
         i++;
    }
    if (i< mynode_get_node_key_count(node) && g_keyEl1_equal_keyEl2[table_id] (g_elementToInsert, mynode_get_node_el(node, i)) ) {
        g_resInsert = -1;
        return NULL;
    }
    if (mynode_is_node_leaf(node)) {
        g_resInsert = 0;
       
        mybptreeIE_shiftDataRightReplEl(node, i, mynode_get_node_key_count_addr(node), g_elementToInsert);

        if (mynode_get_node_key_count(node) == mynode_get_node_order(g_root[table_id])) {
            if (mynode_get_node_num_page (node) == 0) {
                void* right = mybptreeIE_split (table_id, node, level);
                if (right) {
                    mybptreeIE_addNewTreeLevel (table_id, right, level);
                }
            } else {
              return mybptreeIE_split (table_id, node, level);
            }
        }
    } else {
        void* child = mynode_get_node(mynode_get_node_child(node, i), level + 1, false, false, &g_num_page_if_new);
        void* right = mybptreeIE_insertNode (table_id, child, level);

        if (right) {
            count_type key_count = mybptreeIE_shiftDataRightReplEl(node, i, mynode_get_node_key_count_addr(node), g_upshiftElement);
            mybptreeIE_shiftNodeRight(node, i+1, key_count);
            mynode_set_node_child_from_node (node, i+1, right);

            if (key_count == mynode_get_node_order(g_root[table_id])) {
                if (mynode_get_node_num_page (node) == 0) {
                    void* right = mybptreeIE_split (table_id, node,level);
                    if (right) {
                        mybptreeIE_addNewTreeLevel (table_id, right, level);
                    }
                } else {
                   return mybptreeIE_split (table_id, node,level);
                }
            }
        }
    }
    return NULL;
}

void* mybptreeIE_split (int table_id, void* node, int level) {
    void* right = mynode_get_node(NULL, level, true, !mynode_is_node_leaf(node), &g_num_page_if_new);
    mynode_update_root_pages_count(g_root[table_id]);

    mybptreeIE_setNode (right, 0, mynode_is_node_leaf(node), 0, g_num_page_if_new, false);

    memcpy(g_upshiftElement, mynode_get_node_el(node, NODE_MIN[table_id]), NODE_ELEMENT_SIZE[table_id]);

    if (!mynode_is_node_leaf(node)) {
        mynode_erase_node_elKey (node, NODE_MIN[table_id]);
        for (short idx = NODE_MIN[table_id] + 1; idx < mynode_get_node_key_count(node); idx++) {
            mynode_set_node_elKey(right, idx - NODE_MIN[table_id] - 1, mynode_get_node_el(node, idx));
            mynode_erase_node_elKey (node, idx);
            mynode_set_node_key_count (right, mynode_get_node_key_count(right) + 1);
        }
        count_type keyCount = mynode_get_node_key_count(node) - (mynode_get_node_key_count(right) + 1);
        mynode_set_node_key_count(node, keyCount);
    } else {
        for (count_type idx = NODE_MIN[table_id]; idx < mynode_get_node_key_count(node); idx++) {
            mynode_set_node_el(right, idx - NODE_MIN[table_id], mynode_get_node_el(node, idx));
            mynode_erase_node_el(node, idx);
            mynode_set_node_key_count (right, mynode_get_node_key_count(right) + 1);
        }
        mynode_set_node_key_count(node, NODE_MIN[table_id]);
    }

    if (!mynode_is_node_leaf(node)) {
        for (count_type idx = NODE_MIN[table_id] + 1; idx <= mynode_get_node_order(g_root[table_id]); idx++) {
              mynode_set_node_child (right, idx - NODE_MIN[table_id] - 1, mynode_get_node_child(node, idx));
              mynode_set_node_child (node, idx, NULL);
        }
        mynode_set_node_link (right, mynode_get_node_link(node));
        mynode_set_node_link_with_nodeAddr (node, right);
    } else {
        mynode_set_node_link (right, mynode_get_node_link(node));
        mynode_set_node_link_with_nodeAddr (node, right);
    }
    return right;
}

void mybptreeIE_addNewTreeLevel (int table_id, void* right, int level) {
    void* left = mynode_get_node(NULL, level, true, !mynode_is_node_leaf(right), &g_num_page_if_new);
    mynode_convert_from_root (left, g_root[table_id], g_num_page_if_new);

    mynode_resetRoot (g_root[table_id], 1, false, 0, 0);
    mynode_update_root_pages_count(g_root[table_id]);
    mynode_set_node_elKey (g_root[table_id], 0, g_upshiftElement);
    mynode_set_node_child_from_node (g_root[table_id], 0, left);
    mynode_set_node_child_from_node (g_root[table_id], 1, right);
}

int mybptreeIE_eraseBTree(int table_id, void* key, short key_size) {
    mynode_set_table_id (table_id);    
    g_keyToCancel = (void*) malloc(key_size);
    memcpy(g_keyToCancel, key, key_size);

    count_type num_pages = mynode_get_node_pages_count(g_root[table_id]);

    int res = mybptreeIE_eraseNode(table_id, g_root[table_id], g_keyToCancel, 0, NULL, 0);

    if (mynode_get_node_key_count(g_root[table_id]) == 0) {
        if (!mynode_is_node_leaf(g_root[table_id])) {
           void* firstChildRoot = mynode_get_node(mynode_get_node_child(g_root[table_id], 0), 0, false, false, &g_num_page_if_new); 
           mynode_convert_to_root(g_root[table_id], firstChildRoot);
           mynode_set_node_num_page(g_root[table_id], 0);
           mynode_set_node_pages_count(g_root[table_id], num_pages);
           mybptreeIE_dump_page (firstChildRoot);
        }
    }
    mynode_flush_cache_operation ();

    free (g_keyToCancel);
    g_keyToCancel = NULL;

    return res;
}

int mybptreeIE_eraseNode(int table_id, void* node, void* key, int level, void* parent, count_type iParent) {
    level++;
    bool found = false;
    count_type i;
    count_type n = mynode_get_node_key_count (node); 
    bool leaf = mynode_is_node_leaf(node);

    if (leaf) {
        for (i=0; i<n && ( g_key1_equal_keyEl2[table_id] (key, mynode_get_node_el(node, i)) || 
                                                            g_key1_greater_keyEl2[table_id] (key, mynode_get_node_el(node, i)) ); i++) {
            if ( g_key1_equal_keyEl2[table_id] (key, mynode_get_node_el(node, i)) ) {
                found = true;
                break;
            }
        }
    } else {
        for (i=0; i<n && ( g_key1_equal_keyEl2[table_id] (key, mynode_get_node_el(node, i)) || 
                                                            g_key1_greater_keyEl2[table_id] (key, mynode_get_node_el(node, i)) ); i++);
    }

    if (found) {
        n = mybptreeIE_shiftDataLeft (node, i, mynode_get_node_key_count_addr(node));
        if (i==0 && iParent!=0) mynode_set_node_elKey (parent, iParent - 1, mynode_get_node_el (node, i));
        if (mynode_get_node_num_page(node) == 0) {
            return 0;
        } else {
            return n >= NODE_MIN[table_id] ? 0 : 1;
        }
    } else {
        if (leaf) {
           return -1;
        }

        void* childI = mynode_get_node(mynode_get_node_child(node, i), level, false, false, &g_num_page_if_new);

        int res = mybptreeIE_eraseNode(table_id, childI, key, level, node, i);

        if (res<1) {
            return res;
        } else {
            bool hasLeft = i > 0;
            bool hasRight = i < n;
            void* childLeft;
            void* childRight;
            count_type childLeftN =0;
            count_type childRightN=0;
            if (hasLeft) {
                childLeft = mynode_get_node(mynode_get_node_child(node, i-1), level, false, false, &g_num_page_if_new); 
                childLeftN = mynode_get_node_key_count(childLeft); 
            }
            if (hasRight) {
                childRight = mynode_get_node(mynode_get_node_child(node, i+1), level, false, false, &g_num_page_if_new);
                childRightN = mynode_get_node_key_count(childRight);
            }

            if (hasLeft && childLeftN > NODE_MIN[table_id]) {
                if (mynode_is_node_leaf(childLeft)) {
                    mybptreeIE_borrow_leaf(node, i, true);
                } else {
                    mybptreeIE_borrow_internal(node, i, true);
                }
                return 0;
            } else if (hasRight && childRightN > NODE_MIN[table_id]) {
                if (mynode_is_node_leaf(childRight)) {
                    mybptreeIE_borrow_leaf(node, i, false);
                } else {
                    mybptreeIE_borrow_internal(node, i, false);
                }
                return 0;
            } else if (hasLeft) {
                if (!mynode_is_node_leaf(childLeft)) {
                    mybptreeIE_shiftDataRightReplEl(childI, 0, mynode_get_node_key_count_addr (childI), mynode_get_node_el(node, i-1));
                }
                mybptreeIE_mergeChildren(node, i - 1);
                n = mybptreeIE_shiftDataLeft (node, i - 1, mynode_get_node_key_count_addr(node));
                mynode_set_node_child(node, n + 1, NULL);
                mybptreeIE_dump_page (childI);
    
                if (mynode_get_node_num_page (node) == 0) {
                   return 0;
                } else {
                   return n >= NODE_MIN[table_id] ? 0 : 1;
                }
            } else {
                if (!mynode_is_node_leaf(childRight)) {
                    mybptreeIE_shiftDataRightReplEl(childRight, 0, mynode_get_node_key_count_addr(childRight), mynode_get_node_el(node, i));
                }
                mybptreeIE_mergeChildren(node, i);
                n = mybptreeIE_shiftDataLeft (node, i, mynode_get_node_key_count_addr(node));
                mynode_set_node_child(node, n + 1, NULL);
                mybptreeIE_dump_page (childRight);
    
                if (mynode_get_node_num_page (node) == 0) {
                   return 0;
                } else {
                   return n >= NODE_MIN[table_id] ? 0 : 1;
                }
            }

        }
        return -1;
    }
}

void mybptreeIE_mergeChildren (void* node, count_type leftIdx) {
    void* child = mynode_get_node(mynode_get_node_child(node, leftIdx), 99, false, false, &g_num_page_if_new);
    void* sibling =  mynode_get_node(mynode_get_node_child(node, leftIdx + 1), 99, false, false, &g_num_page_if_new);

    count_type childN = mynode_get_node_key_count(child);
    bool childLeaf = mynode_is_node_leaf(child);
    count_type siblingN = mynode_get_node_key_count(sibling);
            
    count_type ogN = childN;
    
    for (count_type i = 0; i < siblingN; i++) {
        if (childLeaf) {
            mynode_set_node_el (child, childN, mynode_get_node_el (sibling, i));
        } else {
            if (i==0) {
                mynode_set_node_elKey (child, childN, mybptreeIE_getSmallestValueSubTree(sibling, 0));
            } else {
                mynode_set_node_elKey (child, childN, mynode_get_node_el (sibling, i));
            }
        }
        childN++;
    }
    mynode_set_node_key_count (child, childN);            

    if (!childLeaf) {
        for (count_type i = 0; i < siblingN; i++) {
            mynode_set_node_child (child, (ogN+1) + i, mynode_get_node_child(sibling, i));
        }
        mynode_set_node_link (child, mynode_get_node_link(sibling));
    } else {
        mynode_set_node_link (child, mynode_get_node_link(sibling));
    }
    mybptreeIE_shiftNodeLeft (node, leftIdx + 1, mynode_get_node_key_count(node) + 1);
}

void mybptreeIE_borrow_leaf (void* node, count_type base, bool toLeft) {
    void* baseNode = mynode_get_node(mynode_get_node_child(node, base), 99, false, false, &g_num_page_if_new);
    bool baseNodeLeaf = mynode_is_node_leaf(baseNode);
    count_type baseNodeN = mynode_get_node_key_count(baseNode);

    if (toLeft) {
        void* leftSiblNode = mynode_get_node(mynode_get_node_child(node, base - 1), 99, false, false, &g_num_page_if_new);
        count_type leftSiblNodeN = mynode_get_node_key_count(leftSiblNode);

        baseNodeN = mybptreeIE_shiftDataRightReplEl(baseNode, 0, mynode_get_node_key_count_addr(baseNode), mynode_get_node_el(leftSiblNode, leftSiblNodeN - 1));

        count_type _n = leftSiblNodeN;
        mynode_set_node_elKey (node, base - 1, mynode_get_node_el (leftSiblNode, leftSiblNodeN - 1));

        mynode_erase_node_el(leftSiblNode, _n - 1);
        mynode_set_node_key_count (leftSiblNode, mynode_get_node_key_count(leftSiblNode) - 1);            

        if (!baseNodeLeaf) {
           mybptreeIE_shiftNodeRight(baseNode, 0, baseNodeN);

           mynode_set_node_child (baseNode, 0, mynode_get_node_child(leftSiblNode, _n));
           mynode_set_node_child (leftSiblNode, _n, NULL);
        }
    } else {
        void* rightSiblNode = mynode_get_node(mynode_get_node_child(node, base + 1), 99, false, false, &g_num_page_if_new);
        count_type rightSiblNodeN = mynode_get_node_key_count(rightSiblNode);

        mynode_set_node_el (baseNode, baseNodeN, mynode_get_node_el (rightSiblNode, 0));
        baseNodeN++;
        mynode_set_node_key_count (baseNode, baseNodeN);            

        count_type _n = rightSiblNodeN;
        rightSiblNodeN = mybptreeIE_shiftDataLeft (rightSiblNode, 0, mynode_get_node_key_count_addr(rightSiblNode));
        mynode_set_node_elKey (node, base, mynode_get_node_el (rightSiblNode, 0));

        if (!baseNodeLeaf) {
           mynode_set_node_child (baseNode, baseNodeN, mynode_get_node_child(rightSiblNode, 0));
           _n++;
           mybptreeIE_shiftNodeLeft (rightSiblNode, 0, _n); 
           mynode_set_node_child (rightSiblNode, _n - 1, NULL);
        }
    }
}

void mybptreeIE_borrow_internal (void* node, count_type base, bool toLeft) {
    void* baseNode = mynode_get_node(mynode_get_node_child(node, base), 99, false, false, &g_num_page_if_new);
    count_type baseNodeN = mynode_get_node_key_count(baseNode);

    if (toLeft) {
        void* leftSiblNode = mynode_get_node(mynode_get_node_child(node, base - 1), 99, false, false, &g_num_page_if_new);
        count_type leftSiblNodeN = mynode_get_node_key_count(leftSiblNode);

        baseNodeN = mybptreeIE_shiftDataRightReplEl(baseNode, 0, mynode_get_node_key_count_addr(baseNode), mynode_get_node_el(node, base - 1));

        count_type _n = leftSiblNodeN;
        mynode_set_node_elKey (node, base - 1 , mynode_get_node_el (leftSiblNode, leftSiblNodeN - 1));

        mynode_erase_node_elKey(leftSiblNode, _n - 1);
        mynode_set_node_key_count (leftSiblNode, mynode_get_node_key_count(leftSiblNode) - 1);            

        mybptreeIE_shiftNodeRight(baseNode, 0, baseNodeN);

        mynode_set_node_child (baseNode, 0, mynode_get_node_child(leftSiblNode, _n));
        mynode_set_node_child (leftSiblNode, _n, NULL);
    } else {
        void* rightSiblNode = mynode_get_node(mynode_get_node_child(node, base + 1), 99, false, false, &g_num_page_if_new);
        count_type rightSiblNodeN = mynode_get_node_key_count(rightSiblNode);

        mynode_set_node_elKey (baseNode, baseNodeN , mynode_get_node_el (node, base));
        baseNodeN++;
        mynode_set_node_key_count (baseNode, baseNodeN);            

        count_type _n = rightSiblNodeN;
        mynode_set_node_elKey (node, base, mynode_get_node_el (rightSiblNode, 0));

        rightSiblNodeN = mybptreeIE_shiftDataLeft (rightSiblNode, 0, mynode_get_node_key_count_addr(rightSiblNode));

        mynode_set_node_child (baseNode, baseNodeN, mynode_get_node_child(rightSiblNode, 0));
        _n++;
        mybptreeIE_shiftNodeLeft (rightSiblNode, 0, _n); 
        mynode_set_node_child (rightSiblNode, _n - 1, NULL);
    }
}

void* mybptreeIE_getSmallestValueSubTree (void* node, count_type idx) {
    void* current = mynode_get_node(mynode_get_node_child(node, idx), 99, false, false, &g_num_page_if_new);
    bool currentLeaf = mynode_is_node_leaf(current);
    count_type currentN = mynode_get_node_key_count(current);

    while (!currentLeaf) {
          current = mynode_get_node(mynode_get_node_child(current, 0), 99, false, false, &g_num_page_if_new);
          currentLeaf = mynode_is_node_leaf(current);
          currentN = mynode_get_node_key_count(current);
    }
    return mynode_get_node_el (current, 0);
}

void mybptreeIE_dump_page (void* node) {
     mynode_dump_page (node);
}

short mybptreeIE_shiftDataRightReplEl (void* node, count_type i, count_type* size, void* newEl) {
    if (*size > 0) {
        for (count_type idx = *size - 1; idx >= i; idx--) {
          if (mynode_is_node_leaf (node)) {
            mynode_set_node_el (node, idx + 1 , mynode_get_node_el (node, idx));
          } else {
            mynode_set_node_elKey (node, idx + 1 , mynode_get_node_el (node, idx));
          }
          if (idx==0) break;
        }
    }
    if (mynode_is_node_leaf (node)) {
        mynode_set_node_el (node, i, newEl);
    } else {
        mynode_set_node_elKey (node, i, newEl);
    }
    return ++(*size);
}

short mybptreeIE_shiftDataLeft (void* node, count_type i, count_type* size) {
    for (; i<*size - 1; i++) {
      if (mynode_is_node_leaf (node)) {
          mynode_set_node_el(node, i, mynode_get_node_el(node, i+1));
      } else {
          mynode_set_node_elKey(node, i, mynode_get_node_el(node, i+1));
      }
    }
    --(*size);
    if (mynode_is_node_leaf (node)) {
        mynode_erase_node_el(node, *size);    
    } else {
        mynode_erase_node_elKey(node, *size);    
    } 
    return *size;
}

int mybptreeIE_shiftNodeRight (void* node, count_type i, count_type size) {
    for (count_type idx = size - 1; idx >= i; idx--) {
        mynode_set_node_child (node, idx + 1, mynode_get_node_child (node, idx));
        if (idx==0) break;
    }
}

int mybptreeIE_shiftNodeLeft (void* node, count_type i, count_type size) {
    for (; i<size-1; i++) {
      mynode_set_node_child (node, i, mynode_get_node_child (node, i+1));
    }
}

void mybptreeIE_setNode (void* node, count_type key_count, bool leaf, count_type pages_count, page_type num_page, bool root) {
    mynode_setNode (node, key_count, leaf, pages_count, num_page, root);
}

void mybptreeIE_set_fn_key_test (int table_id,
                              bool (*keyEl1_greater_keyEl2)(void*, void*), 
                              bool (*keyEl1_equal_keyEl2  )(void*, void*),
                              bool (*key1_greater_keyEl2  )(void*, void*),
                              bool (*key1_equal_keyEl2    )(void*, void*)) {
    g_keyEl1_greater_keyEl2[table_id] = keyEl1_greater_keyEl2;
    g_keyEl1_equal_keyEl2[table_id] = keyEl1_equal_keyEl2;
    g_key1_greater_keyEl2[table_id] = key1_greater_keyEl2;
    g_key1_equal_keyEl2[table_id] = key1_equal_keyEl2;
}

