#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "btree.h"

struct node
{
  size_t n_keys; // amount of keys currently stored
  int *keys;     // array of keys stored (this will be replaced by an struct
                 // representing the data)

  struct node **children; // array of pointers to children

  bool is_leaf; // flag indicating if the node is a leaf node
};

/**
 * The function recursively destroys the node and its children,
 * i.e. frees the memory allocated in the pointer and all children pointers
 *
 * @param node Valid pointer to a node_t variable
 *
 * @post All memory for the node and its children are freed
 */
void node_destroy(node_t *node)
{
  if (!node)
    return;

  if (node->children)
  {
    int i = 0;
    while (node->children[i])
    {
      node_destroy(node->children[i]);
      i++;
    }

    free(node->children);
  }

  if (node->keys)
    free(node->keys);

  free(node);
}

node_t *node_create(bool is_leaf, size_t order)
{
  node_t *new_node = malloc(sizeof(node_t));
  if (!new_node)
  {
    perror("Erro na alocação de memória.");
    exit(EXIT_FAILURE);
  }

  new_node->n_keys = 0;
  new_node->is_leaf = is_leaf;

  new_node->keys = malloc((order - 1) * sizeof(int));
  if (!new_node->keys)
  {
    perror("Erro na alocação de memória.");
    node_destroy(new_node);
    exit(EXIT_FAILURE);
  }

  new_node->children = malloc(order * sizeof(node_t *));
  if (!new_node->children)
  {
    perror("Erro na alocação de memória.");
    node_destroy(new_node);
    exit(EXIT_FAILURE);
  }

  return new_node;
}

int node_keyat(node_t *node, int i)
{
  if (i < 0 || i >= node->n_keys)
    return -1;

  return node->keys[i];
}

node_t *node_search(node_t *node, int key, int *pos)
{
  if (node == NULL)
    return NULL;

  int i = 0;

  while (i < node->n_keys && key > node_keyat(node, i))
    i++;

  /**
   * Se o valor da chave na posição i for o desejado:
   *      Atribui o valor de i à `pos` e retorna o nó
   */
  if (key == node_keyat(node, i))
  {
    *pos = i;
    return node;
  }

  // Mudança necessária: ao invés de pegar o próximo nó (ponteiro) é preciso
  // fazer uma busca no arquivo binário
  /**
   * Nesse caso, o código não faz acessos "out of bounds" em node->children[i]
   * pois o array de filhos possui t elementos, sendo eles ponteiros válidos ou
   * NULL.
   *
   * Como sempre verificamos se i < node->n_keys na função node_keyat, nunca
   * passaremos desse limite.
   */
  return node->children[i] ? node_search(node->children[i], key, pos) : NULL;
}

void node_splitch(node_t *parent, int idx, size_t order)
{
  node_t *child = parent->children[idx]; // y

  node_t *new_node = node_create(child->is_leaf, order); // z

  int min_keys = (order / 2) - 1; // t / 2 - center elem

  new_node->n_keys = min_keys; // n[z] <- t / 2

  for (int i = 0; i < min_keys; i++) // first to left of median
    new_node->keys[i] =
        child->keys[i + min_keys + 1]; // z receives the right part of child

  if (!child->is_leaf)                     // i.e. if z has children
    for (int i = 0; i < min_keys + 1; i++) // first to left of median
      new_node->children[i] = child->children[i + min_keys + 1];

  child->n_keys = min_keys;

  for (int i = parent->n_keys; i > idx;
       i--) // end of parent.keys down to the full child
    parent->children[i + 1] =
        parent->children[i]; // moves all children one to the right

  parent->children[idx + 1] = new_node; // puts the new node in the right place

  for (int i = parent->n_keys - 1; i >= idx; i--)
    parent->keys[i + 1] = parent->keys[i]; // moves all keys one to the right

  parent->keys[idx] = child->keys[min_keys];
  parent->n_keys++;
}

void node_insertnf(node_t *node, int key, size_t order)
{
  int i = node->n_keys - 1;

  if (node->is_leaf)
  {
    while (i >= 0 && node->keys[i] > key)
    {
      node->keys[i + 1] = node->keys[i]; // moves every key one to the right
      i--;
    }

    node->keys[i + 1] = key;
    node->n_keys++;
  }
  else
  {
    while (i >= 0 && node->keys[i] > key)
      i--;

    i++;

    if (node->children[i]->n_keys == order - 1)
    {
      node_splitch(node, i, order);

      if (node->keys[i] < key)
        i++;
    }

    node_insertnf(node->children[i], key, order);
  }
}

// int node_insert(node_t **root, int key, int order)
// {
//   node_t *node = *root;
//   int pos;
//
//   // Check if the key already exists in the tree
//   if (node_search(node, key, &pos))
//     // node->keys[pos] = key; // Update the existing key
//     return 0;
//
//   if (!node) // null node
//   {
//     *root = node_create(true, order);
//     (*root)->keys[0] = key;
//     (*root)->n_keys = 1;
//
//     return 1;
//   }
//   else if (node->n_keys == order - 1) // full node
//   {
//     node_t *new_root = node_create(false, order);
//     new_root->children[0] = node;
//     node_splitch(new_root, 0, order);
//     *root = new_root;
//     node_insertnf(new_root, key, order);
//
//     return 1;
//   }
//
//   node_insertnf(*root, key, order);
//
//   return 0;
// }

int node_insert(node_t **root, int key, int order)
{
  node_t *node = *root;
  int pos;

  if (node_search(node, key, &pos) != NULL)
    return 0;

  if (!node) // nó nulo
  {
    *root = node_create(true, order);
    (*root)->keys[0] = key;
    (*root)->n_keys = 1;

    return 1;
  }
  else if (node->n_keys == order - 1) // nó cheio
  {
    node_t *new_root = node_create(false, order);
    new_root->children[0] = node;
    node_splitch(new_root, 0, order);
    *root = new_root;
    node_insertnf(new_root, key, order);

    return 1;
  }

  node_insertnf(*root, key, order);

  return 0;
}

void node_print(node_t *node)
{
  printf("[ ");
  for (int i = 0; i < node->n_keys; i++)
  {
    printf("key%d: %d", i, node->keys[i]);
    if (i < node->n_keys - 1)
      printf(", ");
  }
  printf(" ]");
}

struct btree
{
  size_t order;   // order of the tree, i.e. the maximum amount of children a
                  // single node has
  node_t *root;   // pointer to the root node of the tree
  size_t n_nodes; // number of nodes the tree currently has
};

btree_t *btree_create(size_t order)
{
  btree_t *tree = malloc(sizeof(btree_t));

  if (!tree)
    return NULL;

  tree->order = order;

  tree->root = NULL;
  tree->n_nodes = 0;

  return tree;
}

void btree_destroy(btree_t *tree)
{
  if (!tree)
    return;

  node_destroy(tree->root);

  free(tree);
}

node_t *btree_search(btree_t *tree, int key, int *pos)
{
  return node_search(tree->root, key, pos);
}

void btree_insert(btree_t *tree, int key)
{
  tree->n_nodes += node_insert(&tree->root, key, tree->order);
}

void btree_print(btree_t *tree)
{
  if (tree->root == NULL)
  {
    printf("Árvore vazia\n");
    return;
  }

  printf("root: ");
  node_print(tree->root);
  printf("\n");

  node_t **queue = calloc(tree->n_nodes, sizeof(node_t *));
  if (!queue)
  {
    perror("Erro na alocação de memória.");
    return;
  }
  int front = 0, rear = 0;
  int level = 1;
  int nodes_curr_lvl = 0;
  int nodes_nxt_lvl = 0;

  if (!tree->root->is_leaf)
  {
    for (int i = 0; i <= tree->root->n_keys; i++)
      if (tree->root->children[i] != NULL)
      {
        queue[rear++] = tree->root->children[i];
        nodes_nxt_lvl++;
      }
  }

  while (front < rear)
  {
    if (nodes_curr_lvl == 0)
    {
      nodes_curr_lvl = nodes_nxt_lvl;
      nodes_nxt_lvl = 0;
      printf("%d-level: ", level++);
    }

    node_t *node = queue[front++];
    nodes_curr_lvl--;

    node_print(node);

    if (!node->is_leaf)
    {
      for (int i = 0; i <= node->n_keys; i++)
      {
        if (node->children[i] != NULL)
        {
          queue[rear++] = node->children[i];
          nodes_nxt_lvl++;
        }
      }
    }

    if (nodes_curr_lvl > 0)
      printf(", ");
    else
      printf("\n");
  }

  free(queue);
}
