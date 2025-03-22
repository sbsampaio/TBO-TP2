#include <cstddef>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "btree.h"

// Códigos de erro para retorno das funções
#define BTREE_SUCCESS 0
#define BTREE_ERROR_ALLOC -1
#define BTREE_ERROR_NOT_FOUND -2
#define BTREE_ERROR_DUPLICATE -3
#define BTREE_ERROR_INVALID_PARAM -4

struct node {
  size_t n_keys; // Quantidade de chaves armazenadas
  int *keys;     // Array de chaves

  struct node **children; // Array de ponteiros para filhos

  bool is_leaf; // Flag indicando se um nó é folha
};

/**
 * Destrói recursivamente o nó e seus filhos
 *
 * @param node Ponteiro válido para o nó a ser destruído
 */
void node_destroy(node_t *node) {
  if (!node)
    return;

  if (node->children) {
    if (!node->is_leaf)
      for (int i = 0; i <= node->n_keys; i++)
        if (node->children[i])
          node_destroy(node->children[i]);

    free(node->children);
  }

  if (node->keys)
    free(node->keys);

  free(node);
}

/**
 * Cria um novo nó e aloca memória para ele
 *
 * @param is_leaf Flag indicando se é um nó folha
 * @param order Ordem da árvore (i.e. quantidade máxima de filhos de um nó)
 *
 * @return Ponteiro para o novo nó ou NULL em caso de erro
 */
node_t *node_create(bool is_leaf, size_t order) {
  if (order < 3)
    return NULL;

  node_t *new_node = malloc(sizeof(node_t));
  if (!new_node)
    return NULL;

  new_node->n_keys = 0;
  new_node->is_leaf = is_leaf;

  new_node->keys = malloc((order - 1) * sizeof(int));
  if (!new_node->keys) {
    free(new_node);
    return NULL;
  }

  new_node->children = malloc(order * sizeof(node_t *));
  if (!new_node->children) {
    free(new_node->keys);
    free(new_node);
    return NULL;
  }

  for (size_t i = 0; i < order; i++)
    new_node->children[i] = NULL;

  return new_node;
}

/**
 * Obtém a chave na posição i do nó
 *
 * @param node Ponteiro para o nó
 * @param i Índice da chave
 *
 * @return A chave na posição i ou -1 se a posição for inválida
 */
int node_keyat(node_t *node, int i) {
  if (!node || i < 0 || i >= node->n_keys)
    return -1;

  return node->keys[i];
}

/**
 * Busca uma chave na árvore
 *
 * @param node Raiz da subárvore onde buscar
 * @param key Chave a ser buscada
 * @param pos Ponteiro para armazenar a posição encontrada
 *
 * @return Ponteiro para o nó contendo a chave ou NULL se não encontrada
 */
node_t *node_search(node_t *node, int key, int *pos) {
  if (!node || !pos)
    return NULL;

  int i = 0;

  while (i < node->n_keys && key > node_keyat(node, i))
    i++;

  // Chave encontrada
  if (i < node->n_keys && key == node_keyat(node, i)) {
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

void node_splitch(node_t *parent, int idx, size_t order) {
  node_t *child = parent->children[idx];

  node_t *new_node = node_create(child->is_leaf, order);

  int mid = order / 2 - 1;
  new_node->n_keys = mid;

  for (int i = 0; i < mid; i++)
    new_node->keys[i] = child->keys[order / 2 + i];

  if (!child->is_leaf)
    for (int i = 0; i < order / 2; i++)
      new_node->children[i] = child->children[order / 2 + i];

  child->n_keys = mid;

  for (int i = parent->n_keys; i > idx; i--)
    parent->children[i + 1] = parent->children[i];

  parent->children[idx + 1] = new_node;

  for (int i = parent->n_keys - 1; i >= idx; i--)
    parent->keys[i + 1] = parent->keys[i];

  parent->keys[idx] = child->keys[mid];
  parent->n_keys++;
}

void node_insertnf(node_t *node, int key, size_t order) {
  int i = node->n_keys - 1;

  if (node->is_leaf) {
    // insert key into sorted order
    while (i >= 0 && key < node->keys[i]) {
      node->keys[i + 1] = node->keys[i];
      i--;
    }

    node->keys[i + 1] = key;
    node->n_keys++;
  } else {
    // find the child to insert key
    while (i >= 0 && key < node->keys[i])
      i--;

    i++;

    if (node->children[i]->n_keys == order - 1) {
      // split child if full
      node_splitch(node, i, order);

      // determine which of two children is the new one
      if (node->keys[i] < key)
        i++;
    }

    node_insertnf(node->children[i], key, order);
  }
}

int node_insert(node_t **root, int key, int order) {
  node_t *node = *root;
  int pos = 0;

  // se a chave já existir na árvore, atualiza o valor
  if (node_search(node, key, &pos) != NULL)
    return 0;

  if (!node) {
    // create new root
    *root = node_create(true, order);
    (*root)->keys[0] = key;
    (*root)->n_keys = 1;

    return 1;
  } else {
    if (node->n_keys == order - 1) {
      // split root if full
      node_t *new_root = node_create(false, order);
      new_root->children[0] = node;
      node_splitch(new_root, 0, order);
      *root = new_root;

      pos = 1;
    }

    node_insertnf(*root, key, order);
  }

  return pos;
}

int node_keyidx(node_t *node, int key) {
  for (int i = 0; i < node->n_keys; i++)
    if (node->keys[i] == key)
      return i;

  return -1;
}

void node_rmleaf(node_t *node, int key, size_t order) {
  if (node->is_leaf)
    return;

  int idx = node_keyidx(node, key);
  if (idx == -1) {
    printf("Key not found on node:");
    node_print(node);
    printf("\n");
    return;
  }

  for (int i = idx; i < node->n_keys; i++)
    node->keys[i] = node->keys[i + 1];
}

node_t *pred(node_t *node, int key, int *p) {
  int idx = node_keyidx(node, key);
  if (idx == -1) {
    printf("Chave não encontrada no nó.\n");
    return NULL;
  }

  node_t *curr = !node->is_leaf ? node->children[idx] : node;
  while (!curr->is_leaf)
    curr = curr->children[curr->n_keys];

  *p = curr->keys[curr->n_keys - 1];
  return curr;
}

node_t *succ(node_t *node, int key, int *s) {
  int idx = node_keyidx(node, key);
  if (idx == -1) {
    printf("Chave não encontrada no nó.\n");
    return NULL;
  }

  node_t *curr = !node->is_leaf ? node->children[idx + 1] : node;
  while (!curr->is_leaf)
    curr = curr->children[0];

  *s = curr->keys[0];
  return curr;
}

void merge(node_t *n, node_t *m) {
  for (int i = 0; i < m->n_keys; i++)
    n->keys[n->n_keys + i] = m->keys[i];

  if (!m->is_leaf)
    for (int i = 0; i <= m->n_keys; i++)
      n->children[n->n_keys + i + 1] = m->children[i];

  n->n_keys += m->n_keys;
  node_destroy(m);
}

int node_remove(node_t *node, int key, size_t order) {
  int idx;
  node_t *n = node_search(node, key, &idx);
  if (!n) {
    printf("Chave não encontrada.\n");
    return 0;
  }

  int t = ceil(order / 2.0);

  /**
   * Caso 1:
   * Chave 'key' está em um nó folha e este nó possui o min. chaves + 1
   *  -> Remove a chave da árvore.
   */
  if (n->is_leaf && n->n_keys >= t)
    node_rmleaf(n, key, order);

  // Caso 2: Chave 'key' está em um nó interno
  if (!n->is_leaf) {
    /**
     * Caso 2.A: O filho à esquerda de 'key' possui pelo menos ceil(order / 2)
     * chaves:
     *    * Encontra pred(key) na subárvore à esquerda, remove pred(key) do nó
     * folha e substitui 'key' por pred(key)
     */
    if (idx <= n->n_keys && n->children[idx]->n_keys >= t) {
      int p;
      node_t *m = pred(node, key, &p);
      node_rmleaf(m, p, order);
      n->keys[idx] = p;
    }
    /**
     * Caso 2.B: O filho à direita de 'key' possui pelo menos ceil(order / 2)
     * chaves:
     *  * Encontra succ(key) na subárvore à direita, remove succ(key) do nó
     * folha e substitui 'key' por succ(key)
     */
    else if (idx < n->n_keys && n->children[idx + 1]->n_keys >= t) {
      int s;
      node_t *m = succ(node, key, &s);
      node_rmleaf(m, s, order);
      n->keys[idx] = s;
    }
    /**
     * Caso 2.C: Ambos possuem o mínimo de chaves em um nó, de modo que remover
     * uma chave viola a propriedade da árvore:
     *  * Junta filho da esquerda e da direita de 'key' e remove a chave de x
     */
    else {
      merge(n->children[idx], n->children[idx + 1]);
      node_remove(n, key, order);
    }
  } else {
    /**
     * Caso 3: Chave não está presente no nó interno 'node':
     *  Determine a subárvore c que contém k. Caso a raíz da subárvore c possua
     * apenas ceil(order / 2) - 1:
     */
    idx = 0;
    while (idx < node->n_keys && key > node->keys[idx])
      idx++;

    n = node->children[idx];

    if (n->n_keys <= t - 1) {
      /**
       * Caso 3.A: A raiz da subárvore c possui pelo menos ceil(order / 2) - 1
       * chaves e um irmão adjascente com ceil(order / 2) chaves:
       *  - Redistribuição
       */
      node_t *m;

      // irmão esquerdo
      if (idx > 0 && node->children[idx - 1]->n_keys >= t) {
        m = node->children[idx - 1];

        node_insertnf(n, node->keys[idx - 1], order);

        node->keys[idx - 1] = m->keys[m->n_keys - 1];
        node_rmleaf(m, m->keys[m->n_keys - 1], order);
      }
      // irmão direito
      else if (idx <= node->n_keys && node->children[idx + 1]->n_keys >= t) {
        m = node->children[idx + 1];

        node_insertnf(n, node->keys[idx], order);

        node->keys[idx + 1] = m->keys[0];
        node_rmleaf(m, m->keys[0], order);
      }
      /**
       * Caso 3.C: A raíz da subárvore e ambos seus irmãos possuem ceil(order /
       * 2) - 1 chaves:
       *  - Concatenação
       */
      else {
        m = node->children[idx + 1];

        node_insertnf(n, node->keys[idx], order);
        merge(n, m);

        for (int i = idx; i < node->n_keys - 1; i++)
          node->keys[idx] = node->keys[idx + 1];
      }
    }
  }

  return 1;
}

// int node_keyidx(node_t *node, int key) {
//   int idx = 0;
//   while (idx < node->n_keys && node->keys[idx] < key)
//     idx++;
//
//   return idx;
// }
//
// /**
//  * Retorna o predecessor de node->keys[idx]
//  *
//  * predecessor: chave mais à direita da subárvore à esquerda de
//  node->keys[idx]
//  */
// int node_pred(node_t *node, int idx) {
//   node_t *curr = node->children[idx];
//   while (!curr->is_leaf)
//     curr = curr->children[curr->n_keys];
//
//   return curr->keys[curr->n_keys - 1];
// }
//
// /**
//  * Retorna o sucessor de node->keys[idx]
//  *
//  * sucessor: chave mais à esquerda da subárvore à direita de node->keys[idx]
//  */
// int node_suc(node_t *node, int idx) {
//   node_t *curr = node->children[idx + 1];
//   while (!curr->is_leaf)
//     curr = curr->children[0];
//
//   return curr->keys[0];
// }
//
// /**
//  * Function to merge node->children[idx] with node->children[idx+1]
//  * node->children[idx+1] is freed after merging
//  */
// void node_merge(node_t *node, int idx, size_t order) {
//   node_t *child = node->children[idx];
//   node_t *brother = node->children[idx + 1];
//
//   int t = ceil(order / 2.0);
//
//   child->keys[t - 1] = node->keys[idx];
//
//   // copiando chaves de node->children[idx+1] para node->children[idx]
//   for (int i = 0; i < brother->n_keys; i++)
//     child->keys[i + t] = brother->keys[i];
//
//   // copiando filhos de node->children[idx+1] para node->children[idx]
//   if (!child->is_leaf)
//     for (int i = 0; i <= brother->n_keys; i++)
//       child->children[i + t] = brother->children[i];
//
//   // moving child pointers after (idx + 1) one step before
//   for (int i = idx + 2; i <= node->n_keys; i++)
//     node->children[i - 1] = node->children[i];
//
//   child->n_keys += brother->n_keys + 1;
//   node->n_keys--;
//
//   node_destroy(brother);
//   return;
// }
//
// void node_brrwprev(node_t *node, int idx) {
//   node_t *child = node->children[idx];
//   node_t *prev = node->children[idx - 1];
//
//   // move todas as chaves em child um índice a frente
//   for (int i = child->n_keys - 1; i >= 0; i--)
//     child->keys[i + 1] = child->keys[i];
//
//   // se child não for folha, move todos os ponteiros um índice a frente
//   if (!child->is_leaf)
//     for (int i = child->n_keys; i >= 0; i--)
//       child->children[i + 1] = child->children[i];
//
//   // primeira chave de child será igual à chave anterior ao idx de node
//   child->keys[0] = node->keys[idx - 1];
//
//   if (!child->is_leaf)
//     child->children[0] = prev->children[prev->n_keys];
//
//   node->keys[idx - 1] = prev->keys[prev->n_keys - 1];
//
//   child->n_keys++;
//   prev->n_keys--;
//
//   return;
// }
//
// void node_brrwnxt(node_t *node, int idx) {
//   node_t *child = node->children[idx];
//   node_t *next = node->children[idx + 1];
//
//   child->keys[child->n_keys] = node->keys[idx];
//
//   if (!child->is_leaf)
//     child->children[child->n_keys + 1] = next->children[0];
//
//   node->keys[idx] = next->keys[0];
//
//   for (int i = 1; i < next->n_keys; i++)
//     next->keys[i - 1] = next->keys[i];
//
//   if (!next->is_leaf)
//     for (int i = 1; i <= next->n_keys; i++)
//       next->children[i - 1] = next->children[i];
//
//   child->n_keys++;
//   next->n_keys--;
//
//   return;
// }
//
// void node_fill(node_t *node, int idx, int order) {
//   int t = ceil(order / 2.0);
//   if (idx != 0 && node->children[idx - 1]->n_keys >= t)
//     node_brrwprev(node, idx);
//   else if (idx != node->n_keys && node->children[idx + 1]->n_keys >= t)
//     node_brrwnxt(node, idx);
//   else {
//     if (idx != node->n_keys)
//       node_merge(node, idx, order);
//     else
//       node_merge(node, idx - 1, order);
//   }
//   return;
// }
//
// int node_remove(node_t *node, int key, size_t order);
//
// void node_rmleaf(node_t *node, int idx) {
//   for (int i = idx + 1; i < node->n_keys; i++)
//     node->keys[i - 1] = node->keys[i];
//
//   node->n_keys--;
//
//   return;
// }
//
// void node_rmint(node_t *node, int idx, size_t order) {
//   int k = node->keys[idx];
//
//   int t = ceil(order / 2.0) - 1;
//
//   // Caso 2.A
//   if (node->children[idx]->n_keys >= t) {
//     int pred = node_pred(node, idx);
//     node->keys[idx] = pred;
//     node_remove(node->children[idx], pred, order);
//   }
//   // Caso 2.B
//   else if (node->children[idx + 1]->n_keys >= t) {
//     int suc = node_suc(node, idx);
//     node->keys[idx] = suc;
//     node_remove(node->children[idx + 1], suc, order);
//   }
//   // Caso 2.C
//   else {
//     node_merge(node, idx, order);
//     node_remove(node->children[idx], k, order);
//   }
//   return;
// }
//
// int node_remove(node_t *root, int key, size_t order) {
//   int idx = node_keyidx(root, key);
//
//   if (idx < root->n_keys && root->keys[idx] == key) {
//     if (root->is_leaf)
//       node_rmleaf(root, idx);
//     else
//       node_rmint(root, idx, order);
//   } else {
//     if (root->is_leaf) {
//       printf("Key not present in tree.\n");
//       return 0;
//     }
//
//     bool flag = idx == root->n_keys;
//     int t = ceil(order / 2.0) - 1;
//
//     if (root->children[idx]->n_keys < t)
//       node_fill(root, idx, order);
//
//     if (flag && idx > root->n_keys)
//       node_remove(root->children[idx - 1], key, order);
//     else
//       node_remove(root->children[idx], key, order);
//   }
//
//   return 1;
// }

void node_print(node_t *node) {
  printf("[ ");
  for (int i = 0; i < node->n_keys; i++) {
    printf("key%d: %d", i, node->keys[i]);
    if (i < node->n_keys - 1)
      printf(", ");
  }
  printf(" ]");
}

struct btree {
  size_t order;   // order of the tree, i.e. the maximum amount of children a
                  // single node has
  node_t *root;   // pointer to the root node of the tree
  size_t n_nodes; // number of nodes the tree currently has
};

btree_t *btree_create(size_t order) {
  btree_t *tree = malloc(sizeof(btree_t));

  if (!tree) {
    perror("Erro na alocação de memória.");
    exit(EXIT_FAILURE);
  }

  tree->order = order;

  tree->root = NULL;
  tree->n_nodes = 0;

  return tree;
}

void btree_destroy(btree_t *tree) {
  if (!tree || !tree->root)
    return;

  node_destroy(tree->root);

  free(tree);
}

node_t *btree_search(btree_t *tree, int key, int *pos) {
  return node_search(tree->root, key, pos);
}

void btree_insert(btree_t *tree, int key) {
  tree->n_nodes += node_insert(&tree->root, key, tree->order);
}

void btree_remove(btree_t *tree, int key) {
  tree->n_nodes -= node_remove(tree->root, key, tree->order);
}

void btree_print(btree_t *tree) {
  if (tree->root == NULL) {
    printf("Árvore vazia\n");
    return;
  }

  printf("root: ");
  node_print(tree->root);
  printf("\n");

  node_t **queue = calloc(tree->n_nodes, sizeof(node_t *));
  if (!queue) {
    perror("Erro na alocação de memória.");
    return;
  }
  int front = 0, rear = 0;
  int level = 1;
  int nodes_curr_lvl = 0;
  int nodes_nxt_lvl = 0;

  if (!tree->root->is_leaf) {
    for (int i = 0; i <= tree->root->n_keys; i++)
      if (tree->root->children[i] != NULL) {
        queue[rear++] = tree->root->children[i];
        nodes_nxt_lvl++;
      }
  }

  while (front < rear) {
    if (nodes_curr_lvl == 0) {
      nodes_curr_lvl = nodes_nxt_lvl;
      nodes_nxt_lvl = 0;
      printf("%d-level: ", level++);
    }

    node_t *node = queue[front++];
    nodes_curr_lvl--;

    node_print(node);

    if (!node->is_leaf) {
      for (int i = 0; i <= node->n_keys; i++) {
        if (node->children[i] != NULL) {
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
