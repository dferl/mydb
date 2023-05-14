#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "node.h"

int mybptree_insertBTree (void* element);
void mybptree_openBTree(const char* filename, int element_size, short order, short key_size);
void mybptree_closeBTree();
int mybptree_eraseBTree(void* key, short key_size);
int mybptree_traverseBTree ();
short mybptree_get_max_order(int element_size, int key_size);
void* mybptree_search_key (void* key, short key_size);
void mybptree_set_fn_key_test (bool (*keyEl1_greater_keyEl2)(void*, void*), 
                                     bool (*keyEl1_equal_keyEl2  )(void*, void*),
                                     bool (*key1_greater_keyEl2  )(void*, void*),
                                     bool (*key1_equal_keyEl2  )(void*, void*));

static void* mybptree_insertNode (void * node, int level);
static void mybptree_setNode (void* node, short key_count, bool leaf, int pages_count, int num_page, bool root);
static void mybptree_addNewTreeLevel (void* right, int level);
static void* mybptree_split (void* node, int level);
static void mybptree_traverseNode (int page);
static void mybptree_traverseNodeDown (void* nodeAddr);
static void mybptree_traverseNodeRight (int page);

static short mybptree_shiftDataRightReplEl (void* node, short i, short* size, void* newEl);
static short mybptree_shiftDataLeft (void* node, short i, short* size);
static int mybptree_shiftNodeRight (void* node, short i, short size);
static int mybptree_shiftNodeLeft (void* node, short i, short size);
static void* mybptree_search_node (void* nodeAddr, void* key);
static int mybptree_eraseNode(void* node, void* key, int level, void* parent, int iParent);
static void mybptree_dump_page (void* node);
static void mybptree_borrow_internal (void* node, short base, bool toLeft);
static void mybptree_borrow_leaf (void* node, short base, bool toLeft);
static void mybptree_mergeChildren (void* node, short leftIdx);

static int g_num_page_if_new;
static int g_resInsert;
static void* g_upshiftElement;
static void* g_elementToInsert;
static void* g_root;
static int g_elementTraversed;
static void* g_keyToCancel;
static void* g_keyToSearch;
static bool (*g_keyEl1_greater_keyEl2)(void*, void*);
static bool (*g_keyEl1_equal_keyEl2)(void*, void*);
static bool (*g_key1_greater_keyEl2)(void*, void*);
static bool (*g_key1_equal_keyEl2)(void*, void*);

static int NODE_ELEMENT_SIZE;
static short TREE_ORDER;
static int NODE_ELEMENTS_BLOCK_OFFSET;
static short NODE_KEY_SIZE;
static short NODE_MIN;

static FILE *fpTrace;

struct node {
  bool  nodeSize;
  short pageSection;
  void* nodePtr;
  void* page;
};

void mybptree_openBTree(const char* filename, int element_size, short order, short key_size) { 
//  INI TRACE
//    fpTrace = fopen ("test_trace_instructions.txt", "w");   
//    fclose (fpTrace);
//  END TRACE

    NODE_KEY_SIZE = key_size;
    NODE_ELEMENT_SIZE = element_size;
    TREE_ORDER = order;
    if ( (order%2) == 0) {
       NODE_MIN = (order/2) - 1;
    } else {
       NODE_MIN = order/2;
    }

    mynode_open_session (NODE_ELEMENT_SIZE, TREE_ORDER, NODE_KEY_SIZE, filename);

    bool created;
    g_root = mynode_get_root (&created);
    if (created) {
        mybptree_setNode (g_root, 0, true, 1, 0, true);
    }

    if (TREE_ORDER != mynode_get_node_order(g_root)) {
        printf("the order parameter is different from the order in the DB root %d, %d \n", TREE_ORDER, mynode_get_node_order(g_root));
        exit(EXIT_FAILURE);
    }

    g_upshiftElement = NULL;
    g_elementToInsert = NULL;
    g_keyToCancel = NULL;
    g_keyToSearch = NULL;
}

void mybptree_closeBTree() {
    mynode_close_session ();

    free(g_upshiftElement);
    free(g_elementToInsert);
    free(g_keyToCancel);
    free(g_keyToSearch);
    g_upshiftElement=NULL;
    g_elementToInsert=NULL;
    g_keyToCancel=NULL;
    g_keyToSearch=NULL;
}

short mybptree_get_max_order(int element_size, int key_size) {
    return mynode_get_max_order(element_size, key_size);
}

int mybptree_insertBTree (void* element) {
//    printf ("mybptree_insertBTree, %d / %d \n", *(int*)element, *(int*)(element+4));
    if (g_upshiftElement == NULL) g_upshiftElement = (void*) malloc(NODE_ELEMENT_SIZE);
    if (g_elementToInsert == NULL) g_elementToInsert = (void*) malloc(NODE_ELEMENT_SIZE);

    g_resInsert = 1;

    memcpy(g_elementToInsert, element, NODE_ELEMENT_SIZE);
    mybptree_insertNode (g_root, 0);
    mynode_flush_cache_operation ();

    return g_resInsert;
}

void* mybptree_insertNode (void * node, int level) {
//    printf ("mybptree_insertNode 0 %d \n", mynode_get_node_num_page(node));
    level++;

    short i = 0;
    while (i< mynode_get_node_key_count(node) && g_keyEl1_greater_keyEl2 (g_elementToInsert, mynode_get_node_el(node, i)) ) {
         i++;
    }
    if (i< mynode_get_node_key_count(node) && g_keyEl1_equal_keyEl2 (g_elementToInsert, mynode_get_node_el(node, i)) ) {
        g_resInsert = -1;
        return NULL;
    }
    if (mynode_is_node_leaf(node)) {
        g_resInsert = 0;
       
        mybptree_shiftDataRightReplEl(node, i, mynode_get_node_key_count_addr(node), g_elementToInsert);

        if (mynode_get_node_key_count(node) == mynode_get_node_order(g_root)) {
            if (mynode_get_node_num_page (node) == 0) {
                void* right = mybptree_split (node, level);
                if (right) {
                    mybptree_addNewTreeLevel (right, level);
                }
            } else {
              return mybptree_split (node, level);
            }
        }
    } else {
    
        void* child = mynode_get_node(mynode_get_node_child(node, i), level + 1, false, false, &g_num_page_if_new);
        void* right = mybptree_insertNode (child, level);

        if (right) {
            short key_count = mybptree_shiftDataRightReplEl(node, i, mynode_get_node_key_count_addr(node), g_upshiftElement);

            mybptree_shiftNodeRight(node, i+1, key_count);
            mynode_set_node_child_from_node (node, i+1, right);

            if (key_count == mynode_get_node_order(g_root)) {
                if (mynode_get_node_num_page (node) == 0) {
                    void* right = mybptree_split (node,level);
                    if (right) {
                        mybptree_addNewTreeLevel (right, level);
                    }
                } else {
                  return mybptree_split (node,level);
                }
            }
        }
    }
    return NULL;
}

void* mybptree_split (void* node, int level) {
    void* right = mynode_get_node(NULL, level, true, !mynode_is_node_leaf(node), &g_num_page_if_new);
    mynode_update_root_pages_count(g_root);

    mybptree_setNode (right, 0, mynode_is_node_leaf(node), 0, g_num_page_if_new, false);

    memcpy(g_upshiftElement, mynode_get_node_el(node, NODE_MIN), NODE_ELEMENT_SIZE);

    if (!mynode_is_node_leaf(node)) {
        mynode_erase_node_elKey (node, NODE_MIN);
        for (short idx = NODE_MIN + 1; idx < mynode_get_node_key_count(node); idx++) {
            mynode_set_node_elKey(right, idx - NODE_MIN - 1, mynode_get_node_el(node, idx));
            mynode_erase_node_elKey (node, idx);
            mynode_set_node_key_count (right, mynode_get_node_key_count(right) + 1);
        }
        short keyCount = mynode_get_node_key_count(node) - (mynode_get_node_key_count(right) + 1);
        mynode_set_node_key_count(node, keyCount);
    } else {
        for (short idx = NODE_MIN; idx < mynode_get_node_key_count(node); idx++) {
            mynode_set_node_el(right, idx - NODE_MIN, mynode_get_node_el(node, idx));
            mynode_erase_node_el(node, idx);
            mynode_set_node_key_count (right, mynode_get_node_key_count(right) + 1);
        }
        mynode_set_node_key_count(node, NODE_MIN);
    }

    if (!mynode_is_node_leaf(node)) {
        for (short idx = NODE_MIN + 1; idx <= mynode_get_node_order(g_root); idx++) {
              mynode_set_node_child (right, idx - NODE_MIN - 1, mynode_get_node_child(node, idx));
              mynode_set_node_child (node, idx, NULL);
        }
    } else {
        mynode_set_node_leaf_link (right, mynode_get_node_leaf_link(node));
        mynode_set_node_leaf_link (node, mynode_get_node_num_page(right));
    }
    return right;
}

void mybptree_addNewTreeLevel (void* right, int level) {
    void* left = mynode_get_node(NULL, level, true, !mynode_is_node_leaf(right), &g_num_page_if_new);
    mynode_convert_from_root (left, g_root, g_num_page_if_new);

    mybptree_setNode (g_root, 1, false, 0, 0, true);
    mynode_update_root_pages_count(g_root);
    mynode_set_node_elKey (g_root, 0, g_upshiftElement);
    mynode_set_node_child_from_node (g_root, 0, left);
    mynode_set_node_child_from_node (g_root, 1, right);
}

int mybptree_eraseBTree(void* key, short key_size) {
    if (g_keyToCancel == NULL) g_keyToCancel = (void*) malloc(key_size);
    memcpy(g_keyToCancel, key, key_size);

    int num_pages = mynode_get_node_pages_count(g_root);

    int res = mybptree_eraseNode(g_root, g_keyToCancel, 0, NULL, 0);

    if (mynode_get_node_key_count(g_root) == 0) {
        if (!mynode_is_node_leaf(g_root)) {
           void* firstChildRoot = mynode_get_node(mynode_get_node_child(g_root, 0), 0, false, false, &g_num_page_if_new); 
           mynode_convert_to_root(g_root, firstChildRoot);
           mynode_set_node_num_page(g_root, 0);
           mynode_set_node_pages_count(g_root, num_pages);
           mybptree_dump_page (firstChildRoot);
        }
    }
    mynode_flush_cache_operation ();

    return res;
}

int mybptree_eraseNode(void* node, void* key, int level, void* parent, int iParent) {
    level++;
    bool found = false;
    short i;
    short n = mynode_get_node_key_count (node); 
    bool leaf = mynode_is_node_leaf(node);

    if (leaf) {
        for (i=0; i<n && ( g_key1_equal_keyEl2 (key, mynode_get_node_el(node, i)) || g_key1_greater_keyEl2 (key, mynode_get_node_el(node, i)) ); i++) {
            if ( g_key1_equal_keyEl2 (key, mynode_get_node_el(node, i)) ) {
                found = true;
                break;
            }
        }
    } else {
        for (i=0; i<n && ( g_key1_equal_keyEl2 (key, mynode_get_node_el(node, i)) || g_key1_greater_keyEl2 (key, mynode_get_node_el(node, i)) ); i++);
    }

    if (found) {
        n = mybptree_shiftDataLeft (node, i, mynode_get_node_key_count_addr(node));
        if (i==0 && iParent!=0) mynode_set_node_elKey (parent, iParent - 1, mynode_get_node_el (node, i));
        if (mynode_get_node_num_page(node) == 0) {
            return 0;
        } else {
            return n >= NODE_MIN ? 0 : 1;
        }
    } else {
        if (leaf) {
           return -1;
        }

        void* childI = mynode_get_node(mynode_get_node_child(node, i), level, false, false, &g_num_page_if_new);

        int res = mybptree_eraseNode(childI, key, level, node, i);

        if (res<1) {
            return res;
        } else {
            bool hasLeft = i > 0;
            bool hasRight = i < n;
            void* childLeft;
            void* childRight;
            short childLeftN, childRightN;
            if (hasLeft) {
                childLeft = mynode_get_node(mynode_get_node_child(node, i-1), level, false, false, &g_num_page_if_new); 
                childLeftN = mynode_get_node_key_count(childLeft); 
            }
            if (hasRight) {
                childRight = mynode_get_node(mynode_get_node_child(node, i+1), level, false, false, &g_num_page_if_new);
                childRightN = mynode_get_node_key_count(childRight);
            }

            if (hasLeft && childLeftN > NODE_MIN) {
                if (mynode_is_node_leaf(childLeft)) {
                    mybptree_borrow_leaf(node, i, true);
                } else {
                    mybptree_borrow_internal(node, i, true);
                }
                return 0;
            } else if (hasRight && childRightN > NODE_MIN) {
                if (mynode_is_node_leaf(childRight)) {
                    mybptree_borrow_leaf(node, i, false);
                } else {
                    mybptree_borrow_internal(node, i, false);
                }
                return 0;
            } else if (hasLeft) {
                if (!mynode_is_node_leaf(childLeft)) {
                    mybptree_shiftDataRightReplEl(childI, 0, mynode_get_node_key_count_addr (childI), mynode_get_node_el(node, i-1));
                }
                mybptree_mergeChildren(node, i - 1);
                n = mybptree_shiftDataLeft (node, i - 1, mynode_get_node_key_count_addr(node));
                mynode_set_node_child(node, n + 1, NULL);
                mybptree_dump_page (childI);
    
                if (mynode_get_node_num_page (node) == 0) {
                   return 0;
                } else {
                   return n >= NODE_MIN ? 0 : 1;
                }
            } else {
                if (!mynode_is_node_leaf(childRight)) {
                    mybptree_shiftDataRightReplEl(childRight, 0, mynode_get_node_key_count_addr(childRight), mynode_get_node_el(node, i));
                }
                mybptree_mergeChildren(node, i);
                n = mybptree_shiftDataLeft (node, i, mynode_get_node_key_count_addr(node));
                mynode_set_node_child(node, n + 1, NULL);
                mybptree_dump_page (childRight);
    
                if (mynode_get_node_num_page (node) == 0) {
                   return 0;
                } else {
                   return n >= NODE_MIN ? 0 : 1;
                }
            }

        }
        return -1;
    }
}

void mybptree_mergeChildren (void* node, short leftIdx) {
    void* child = mynode_get_node(mynode_get_node_child(node, leftIdx), 99, false, false, &g_num_page_if_new);
    void* sibling =  mynode_get_node(mynode_get_node_child(node, leftIdx + 1), 99, false, false, &g_num_page_if_new);

    short childN = mynode_get_node_key_count(child);
    bool childLeaf = mynode_is_node_leaf(child);
    short siblingN = mynode_get_node_key_count(sibling);
            
    short ogN = childN;
    
    for (int i = 0; i < siblingN; i++) {
        if (childLeaf) {
            mynode_set_node_el (child, childN, mynode_get_node_el (sibling, i));
        } else {
            if (i==0) {
                void* leftMostchildOfSibling = mynode_get_node(mynode_get_node_child(sibling, 0), 99, false, false, &g_num_page_if_new);
                mynode_set_node_elKey (child, childN, mynode_get_node_el (leftMostchildOfSibling, 0));
            } else {
                mynode_set_node_elKey (child, childN, mynode_get_node_el (sibling, i));
            }
        }
        childN++;
    }
    mynode_set_node_key_count (child, childN);            

    if (!childLeaf) {
        for (int i = 0; i < siblingN; i++) {
            mynode_set_node_child (child, (ogN+1) + i, mynode_get_node_child(sibling, i));
        }
    } else {
        mynode_set_node_leaf_link (child, mynode_get_node_leaf_link(sibling));
    }
    mybptree_shiftNodeLeft (node, leftIdx + 1, mynode_get_node_key_count(node) + 1);
}

void mybptree_borrow_leaf (void* node, short base, bool toLeft) {
    void* baseNode = mynode_get_node(mynode_get_node_child(node, base), 99, false, false, &g_num_page_if_new);
    bool baseNodeLeaf = mynode_is_node_leaf(baseNode);
    short baseNodeN = mynode_get_node_key_count(baseNode);

    if (toLeft) {
        void* leftSiblNode = mynode_get_node(mynode_get_node_child(node, base - 1), 99, false, false, &g_num_page_if_new);
        short leftSiblNodeN = mynode_get_node_key_count(leftSiblNode);

        baseNodeN = mybptree_shiftDataRightReplEl(baseNode, 0, mynode_get_node_key_count_addr(baseNode), mynode_get_node_el(leftSiblNode, leftSiblNodeN - 1));

        short _n = leftSiblNodeN;
        mynode_set_node_elKey (node, base - 1, mynode_get_node_el (leftSiblNode, leftSiblNodeN - 1));

        mynode_erase_node_el(leftSiblNode, _n - 1);
        mynode_set_node_key_count (leftSiblNode, mynode_get_node_key_count(leftSiblNode) - 1);            

        if (!baseNodeLeaf) {
           mybptree_shiftNodeRight(baseNode, 0, baseNodeN);

           mynode_set_node_child (baseNode, 0, mynode_get_node_child(leftSiblNode, _n));
           mynode_set_node_child (leftSiblNode, _n, NULL);
        }
    } else {
        void* rightSiblNode = mynode_get_node(mynode_get_node_child(node, base + 1), 99, false, false, &g_num_page_if_new);
        short rightSiblNodeN = mynode_get_node_key_count(rightSiblNode);

        mynode_set_node_el (baseNode, baseNodeN, mynode_get_node_el (rightSiblNode, 0));
        baseNodeN++;
        mynode_set_node_key_count (baseNode, baseNodeN);            

        int _n = rightSiblNodeN;
        rightSiblNodeN = mybptree_shiftDataLeft (rightSiblNode, 0, mynode_get_node_key_count_addr(rightSiblNode));
        mynode_set_node_elKey (node, base, mynode_get_node_el (rightSiblNode, 0));

        if (!baseNodeLeaf) {
           mynode_set_node_child (baseNode, baseNodeN, mynode_get_node_child(rightSiblNode, 0));
           _n++;
           mybptree_shiftNodeLeft (rightSiblNode, 0, _n); 
           mynode_set_node_child (rightSiblNode, _n - 1, NULL);
        }
    }
}

void mybptree_borrow_internal (void* node, short base, bool toLeft) {
    void* baseNode = mynode_get_node(mynode_get_node_child(node, base), 99, false, false, &g_num_page_if_new);
    short baseNodeN = mynode_get_node_key_count(baseNode);

    if (toLeft) {
        void* leftSiblNode = mynode_get_node(mynode_get_node_child(node, base - 1), 99, false, false, &g_num_page_if_new);
        short leftSiblNodeN = mynode_get_node_key_count(leftSiblNode);

        baseNodeN = mybptree_shiftDataRightReplEl(baseNode, 0, mynode_get_node_key_count_addr(baseNode), mynode_get_node_el(node, base - 1));

        short _n = leftSiblNodeN;
        mynode_set_node_elKey (node, base - 1 , mynode_get_node_el (leftSiblNode, leftSiblNodeN - 1));

        mynode_erase_node_el(leftSiblNode, _n - 1);
        mynode_set_node_key_count (leftSiblNode, mynode_get_node_key_count(leftSiblNode) - 1);            

        mybptree_shiftNodeRight(baseNode, 0, baseNodeN);

        mynode_set_node_child (baseNode, 0, mynode_get_node_child(leftSiblNode, _n));
        mynode_set_node_child (leftSiblNode, _n, NULL);
    } else {
        void* rightSiblNode = mynode_get_node(mynode_get_node_child(node, base + 1), 99, false, false, &g_num_page_if_new);
        short rightSiblNodeN = mynode_get_node_key_count(rightSiblNode);

        mynode_set_node_elKey (baseNode, baseNodeN , mynode_get_node_el (node, base));
        baseNodeN++;
        mynode_set_node_key_count (baseNode, baseNodeN);            

        int _n = rightSiblNodeN;
        mynode_set_node_elKey (node, base, mynode_get_node_el (rightSiblNode, 0));

        rightSiblNodeN = mybptree_shiftDataLeft (rightSiblNode, 0, mynode_get_node_key_count_addr(rightSiblNode));

        mynode_set_node_child (baseNode, baseNodeN, mynode_get_node_child(rightSiblNode, 0));
        _n++;
        mybptree_shiftNodeLeft (rightSiblNode, 0, _n); 
        mynode_set_node_child (rightSiblNode, _n - 1, NULL);
    }
}

void mybptree_dump_page (void* node) {
     mynode_dump_page (node);
}

short mybptree_shiftDataRightReplEl (void* node, short i, short* size, void* newEl) {
    for (int idx = *size - 1; idx >= i; idx--) {
      if (mynode_is_node_leaf (node)) {
          mynode_set_node_el (node, idx + 1 , mynode_get_node_el (node, idx));
      } else {
          mynode_set_node_elKey (node, idx + 1 , mynode_get_node_el (node, idx));
      }
    }
    if (mynode_is_node_leaf (node)) {
        mynode_set_node_el (node, i, newEl);
    } else {
        mynode_set_node_elKey (node, i, newEl);
    }
    return ++(*size);
}

short mybptree_shiftDataLeft (void* node, short i, short* size) {
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

int mybptree_shiftNodeRight (void* node, short i, short size) {
    for (int idx = size - 1; idx >= i; idx--) {
       mynode_set_node_child (node, idx + 1, mynode_get_node_child (node, idx));
    }
}

int mybptree_shiftNodeLeft (void* node, short i, short size) {
    for (; i<size-1; i++) {
      mynode_set_node_child (node, i, mynode_get_node_child (node, i+1));
    }
}

void mybptree_setNode (void* node, short key_count, bool leaf, int pages_count, int num_page, bool root) {
    mynode_setNode (node, key_count, leaf, pages_count, num_page, root);
}

void mybptree_set_fn_key_test (bool (*keyEl1_greater_keyEl2)(void*, void*), 
                              bool (*keyEl1_equal_keyEl2  )(void*, void*),
                              bool (*key1_greater_keyEl2  )(void*, void*),
                              bool (*key1_equal_keyEl2    )(void*, void*)) {
    g_keyEl1_greater_keyEl2 = keyEl1_greater_keyEl2;
    g_keyEl1_equal_keyEl2 = keyEl1_equal_keyEl2;
    g_key1_greater_keyEl2 = key1_greater_keyEl2;
    g_key1_equal_keyEl2 = key1_equal_keyEl2;
}

int mybptree_traverseBTree () {
     g_elementTraversed = 0;
     mybptree_traverseNodeDown(mynode_get_root_nodeAddr());
     return g_elementTraversed;
}

void mybptree_traverseNodeDown (void* nodeAddr) {
     int page = mynode_get_page_from_address (nodeAddr);
     int i;
     void* node = mynode_get_node_for_traverse_by_addr(nodeAddr, &i);

     bool leaf = mynode_is_node_leaf (node);
     void* leftMostChildAddr;
     if (!leaf) {
        leftMostChildAddr = mynode_get_node_child(node, 0);
     }
     mynode_free_page (i);
     if (leaf) {
        mybptree_traverseNodeRight (page);
     } else {
        mybptree_traverseNodeDown (leftMostChildAddr);
     }
}

void mybptree_traverseNodeRight (int page) {
     int currentPage = page;
     int currentI;
     int currentNode_leaf_link;
     void* currentNode;
     short currentNodeN;

     while (true) {
        currentNode = mynode_get_node_for_traverse_by_page(currentPage, &currentI);
        currentNodeN = mynode_get_node_key_count(currentNode);

        for (int i=0; i < currentNodeN; i++) {
            g_elementTraversed++;
        }

        currentNode_leaf_link = mynode_get_node_leaf_link (currentNode);
        if (currentNode_leaf_link == 0) break;
        currentPage = currentNode_leaf_link;
        mynode_free_page (currentI);
     }
 }

void* mybptree_search_key (void* key, short key_size) {
    if (g_keyToSearch == NULL) g_keyToSearch = (void*) malloc(key_size);
    memcpy(g_keyToSearch, key, key_size);

    return mybptree_search_node (mynode_get_root_nodeAddr(), key);
}

void* mybptree_search_node (void* nodeAddr, void* key) {
    int iPager;
    void* node = mynode_get_node_for_traverse_by_addr(nodeAddr, &iPager);
    short n = mynode_get_node_key_count (node); 
    short i;
    bool leaf = mynode_is_node_leaf(node);
    bool found = false;

    if (leaf) {
        for (i=0; i<n && ( g_key1_equal_keyEl2 (key, mynode_get_node_el(node, i)) || g_key1_greater_keyEl2 (key, mynode_get_node_el(node, i)) ); i++) {
            if (g_key1_equal_keyEl2 (key, mynode_get_node_el(node, i))) {
                found = true;
                break;
            }
        }
    } else {
        for (i=0; i<n && ( g_key1_equal_keyEl2 (key, mynode_get_node_el(node, i)) || g_key1_greater_keyEl2 (key, mynode_get_node_el(node, i)) ); i++);
    }

    if (leaf && found) {
        return mynode_get_node_el(node, i);
    } else if (leaf && !found) {
        return NULL;
    } else {
        mynode_save_nodeAddr (mynode_get_node_child(node, i));
        mynode_free_page (iPager);
        return mybptree_search_node (mynode_get_saved_nodeAddr(), key);
    }
}


