#ifndef BTREE_H
#define BTREE_H

#include <stdlib.h>

typedef struct node node_t;

typedef struct btree btree_t;

void node_print(node_t* node);

/**
 * The function allocates, dinamically, a pointer to the type btree
 *
 * If some error happens during the allocation, the function returns NULL
 *
 * @param order The order, or the amount of children, a single node has
 *
 * @return The pointer allocated
 */
btree_t* btree_create(size_t order);

/**
 * The function recursively destroys the B Tree
 * In other words, the function frees the memory
 * allocated related to the pointer and all nodes inside the tree
 *
 * @param tree A valid pointer to a btree_t variable
 *
 * @post The memory allocated in the pointer is freed, and also all
 * memory of the nodes inside the tree
 */
void btree_destroy(btree_t* tree);

/**
 * The function recursively searches all nodes until the one with the key is found
 *
 * If the key is not in any node in the tree, the function returns NULL
 *
 * @param tree A valid pointer to a btree_t variable
 * @param key Key to search
 * @param pos A pointer to an integer that after the function is executed,
 * it contains the index of the key found, -1 if the key is not found
 *
 * @return A pointer to the node where the key was found
 */
node_t* btree_search(btree_t* tree, int key, int* pos);

void btree_insert(btree_t* tree, int key);

void btree_remove(btree_t* tree, int key);

void btree_print(btree_t* tree);

#endif // !BTREE_H
