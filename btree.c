#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "btree.h"

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

  if (node->is_leaf)
    return NULL;

  // Busca recursivamente no filho apropriado
  return node_search(node->children[i], key, pos);
}

/**
 * Divide um nó filho quando está cheio
 *
 * @param parent Nó pai
 * @param idx Índice do filho a ser dividido
 * @param order Ordem da árvore
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_split_child(node_t *parent, int idx, size_t order) {
  if (!parent || idx < 0 || idx > parent->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  node_t *child = parent->children[idx];
  if (!child)
    return BTREE_ERROR_INVALID_PARAM;

  node_t *new_node = node_create(child->is_leaf, order);
  if (!new_node)
    return BTREE_ERROR_ALLOC;

  int mid = order / 2 - 1;
  new_node->n_keys = mid; // Metade das chaves vai para o novo nó

  // Copia as chaves do nó filho para novo nó
  for (int i = 0; i < mid; i++)
    new_node->keys[i] = child->keys[mid + 1 + i];

  // Se não for folha, copia os ponteiros para filhos também
  if (!child->is_leaf)
    for (int i = 0; i < order / 2; i++)
      new_node->children[i] = child->children[mid + 1 + i];

  // Atualiza o número de chaves no filho
  child->n_keys = mid;

  // Move os ponteiros para filhos no pai para abrir espaço
  for (int i = parent->n_keys; i > idx; i--)
    parent->children[i + 1] = parent->children[i];

  // Liga o novo nó ao pai
  parent->children[idx + 1] = new_node;

  // Move as chaves no pai para abrir espaço
  for (int i = parent->n_keys - 1; i >= idx; i--)
    parent->keys[i + 1] = parent->keys[i];

  // Insere a chave do meio do filho no pai
  parent->keys[idx] = child->keys[mid];
  parent->n_keys++;

  return BTREE_SUCCESS;
}

/**
 * Insere uma chave em um nó não cheio
 *
 * @param node Nó onde inserir
 * @param key Chave a ser inserida
 * @param order Ordem da árvore
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_insert_non_full(node_t *node, int key, size_t order) {
  if (!node)
    return BTREE_ERROR_INVALID_PARAM;

  int i = node->n_keys - 1;

  if (node->is_leaf) {
    // Encontra a posição correta e insere a chave
    while (i >= 0 && key < node->keys[i]) {
      node->keys[i + 1] = node->keys[i];
      i--;
    }

    node->keys[i + 1] = key;
    node->n_keys++;

    return BTREE_SUCCESS;
  } else {
    // Encontra o filho onde a chave deve ser inserida
    while (i >= 0 && key < node->keys[i])
      i--;

    i++;

    // Divide o filho, se cheio
    if (node->children[i]->n_keys == order - 1) {
      int result = node_split_child(node, i, order);
      if (result != BTREE_SUCCESS)
        return result;

      // Decide qual dos filhos vai conter a chave
      if (node->keys[i] < key)
        i++;
    }

    // Insere a chave recursivamente no filho apropriado
    return node_insert_non_full(node->children[i], key, order);
  }
}

/**
 * Insere uma chave na árvore
 *
 * @param root Ponteiro para o ponteiro da raiz
 * @param key Chave a ser inserida
 * @param order Ordem da árvore
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_insert(node_t **root, int key, int order) {
  if (!root)
    return BTREE_ERROR_INVALID_PARAM;

  // Verifica se chave já existe
  int pos;
  if (*root && node_search(*root, key, &pos) != NULL)
    return BTREE_ERROR_DUPLICATE;

  // Se a raiz for NULL, cria uma nova raiz
  if (!(*root)) {
    *root = node_create(true, order);
    if (!(*root))
      return BTREE_ERROR_ALLOC;

    (*root)->keys[0] = key;
    (*root)->n_keys = 1;

    return BTREE_SUCCESS;
  }

  // Se raiz estiver cheia, cria nova raiz
  if ((*root)->n_keys == order - 1) {
    node_t *new_root = node_create(false, order);
    if (!new_root)
      return BTREE_ERROR_ALLOC;

    new_root->children[0] = *root;
    *root = new_root;

    int result = node_split_child(new_root, 0, order);
    if (result != BTREE_SUCCESS) {
      new_root->children[0] = NULL;
      node_destroy(new_root);
      return result;
    }

    return node_insert_non_full(*root, key, order);
  }

  return node_insert_non_full(*root, key, order);
}

/**
 * Encontra o índice de uma chave em um nó
 *
 * @param node Nó onde buscar
 * @param key Chave a ser buscada
 *
 * @return Índice da chave ou -1 se não encontrada
 */
int node_key_idx(node_t *node, int key) {
  if (!node)
    return -1;

  for (int i = 0; i < node->n_keys; i++)
    if (node->keys[i] == key)
      return i;

  return -1;
}

/**
 * Encontra o predecessor de uma chave
 *
 * @param node Nó contendo a chave
 * @param idx Índice da chave
 * @param pred Ponteiro para armazenar o predecessor
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_predecessor(node_t *node, int idx, int *pred) {
  if (!node || !pred || idx < 0 || idx >= node->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  if (node->is_leaf)
    return BTREE_ERROR_INVALID_PARAM;

  // Vai para o filho à esquerda da chave
  node_t *curr = node->children[idx];
  if (!curr)
    return BTREE_ERROR_INVALID_PARAM;

  // Desce até o elemento mais à direita (maior valor)
  while (!curr->is_leaf) {
    if (!curr->children[curr->n_keys])
      return BTREE_ERROR_INVALID_PARAM;

    curr = curr->children[curr->n_keys];
  }

  if (curr->n_keys <= 0)
    return BTREE_ERROR_INVALID_PARAM;

  *pred = curr->keys[curr->n_keys - 1];
  return BTREE_SUCCESS;
}

/**
 * Encontra o sucessor de uma chave
 *
 * @param node Nó contendo a chave
 * @param idx Índice da chave
 * @param succ Ponteiro para armazenar o sucessor
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_successor(node_t *node, int idx, int *succ) {
  if (!node || !succ || idx < 0 || idx >= node->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  if (node->is_leaf)
    return BTREE_ERROR_INVALID_PARAM;

  // Vai para o filho à direita da chave
  node_t *curr = node->children[idx + 1];
  if (!curr)
    return BTREE_ERROR_INVALID_PARAM;

  // Desce até o elemento mais à esquerda (menor valor)
  while (!curr->is_leaf) {
    if (!curr->children[0])
      return BTREE_ERROR_INVALID_PARAM;

    curr = curr->children[0];
  }

  if (curr->n_keys <= 0)
    return BTREE_ERROR_INVALID_PARAM;

  *succ = curr->keys[0];
  return BTREE_SUCCESS;
}

/**
 * Mescla dois nós filhos de um nó
 *
 * @param parent Nó pai
 * @param idx Índice do primeiro filho
 * @param order Ordem da árvore
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_merge(node_t *parent, int idx, size_t order) {
  if (!parent || idx < 0 || idx >= parent->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  node_t *l_child = parent->children[idx];
  node_t *r_child = parent->children[idx + 1];

  if (!l_child || !r_child)
    return BTREE_ERROR_INVALID_PARAM;

  int t = ceil(order / 2.0);

  // Move a chave do pai para o filho à esquerda
  l_child->keys[l_child->n_keys] = parent->keys[idx];

  // Move todas as chaves do filho à direita para o filho à esquerda
  for (int i = 0; i < r_child->n_keys; i++)
    l_child->keys[l_child->n_keys + 1 + i] = r_child->keys[i];

  // Move também ponteiros para filhos, se não for folha
  if (!l_child->is_leaf)
    for (int i = 0; i <= r_child->n_keys; i++)
      l_child->children[l_child->n_keys + 1 + i] = r_child->children[i];

  // Atualiza número de chaves filho à esquerda
  l_child->n_keys += r_child->n_keys + 1;

  // Remove chave do pai e ajusta ponteiros
  for (int i = idx; i < parent->n_keys - 1; i++) {
    parent->keys[i] = parent->keys[i + 1];
    parent->children[i + 1] = parent->children[i + 2];
  }

  parent->n_keys--;

  // Libera memória do filho à direita
  node_destroy(r_child);

  return BTREE_SUCCESS;
}

/**
 * Remove uma chave de um nó folha
 *
 * @param node Nó de onde remover
 * @param idx Índice da chave
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_remove_from_leaf(node_t *node, int idx) {
  if (!node || idx < 0 || idx >= node->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  // Move todas as chaves à frente de node->keys[idx] uma posição para trás
  for (int i = idx; i < node->n_keys - 1; i++)
    node->keys[i] = node->keys[i + 1];

  node->n_keys--;
  return BTREE_SUCCESS;
}

/**
 * Remove uma chave da árvore
 *
 * @param node Raiz da subárvore onde buscar e remover
 * @param key Chave a ser removida
 * @param order Ordem da árvore
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_remove(node_t *node, int key, size_t order);

/**
 * Remove uma chave de um nó interno
 *
 * @param node Nó de onde remover
 * @param idx Índice da chave
 * @param order Ordem da chave
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_remove_from_internal(node_t *node, int idx, size_t order) {
  if (!node || idx < 0 || idx >= node->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  int key = node->keys[idx];
  int t = ceil(order / 2.0);

  // Caso 2a: O filho à esquerda tem pelo menos t chaves
  if (node->children[idx]->n_keys >= t) {
    int pred;
    if (node_predecessor(node, idx, &pred) != BTREE_SUCCESS)
      return BTREE_ERROR_INVALID_PARAM;

    node->keys[idx] = pred;

    // Remove recursivamente o predecessor
    return node_remove(node->children[idx], pred, order);
  }
  // Caso 2b: O filho à direita tem pelo menos t chaves
  else if (node->children[idx + 1]->n_keys >= t) {
    int succ;
    if (node_successor(node, idx, &succ) != BTREE_SUCCESS)
      return BTREE_ERROR_INVALID_PARAM;

    node->keys[idx] = succ;

    // Remove recursivamente o sucessor
    return node_remove(node->children[idx + 1], succ, order);
  }

  // Caso 2c: Ambos os filhos têm menos de t chaves

  // Mescla os filhos e depois remove a chave do filho mesclado
  int result = node_merge(node, idx, order);
  if (result != BTREE_SUCCESS)
    return result;

  return node_remove(node->children[idx], key, order);
}

/**
 * Garante que o filho na posição idx tenha pelo menos ⌈order/2⌉ - 1 chaves
 *
 * @param node Nó pai
 * @param idx Índice do filho
 * @param order Ordem da árvore
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_ensure_min_keys(node_t *node, int idx, size_t order) {
  if (!node || idx < 0 || idx > node->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  node_t *child = node->children[idx];
  if (!child)
    return BTREE_ERROR_INVALID_PARAM;

  int t = ceil(order / 2.0);

  if (child->n_keys >= t)
    return BTREE_SUCCESS;

  // Caso 3a-esq: Empresta uma chave do irmão à esquerda
  if (idx > 0 && node->children[idx - 1]->n_keys >= t) {
    node_t *l_sibling = node->children[idx - 1];

    // Move todas as chaves uma posição para a direita
    for (int i = child->n_keys - 1; i >= 0; i--)
      child->keys[i + 1] = child->keys[i];

    // Move os ponteiros para filhos também, se não for folha
    if (!child->is_leaf)
      for (int i = child->n_keys; i >= 0; i--)
        child->children[i + 1] = child->children[i];

    // Move a chave do pai para a primeira posição da chave filha
    child->keys[0] = node->keys[idx - 1];

    // O primeiro filho recebe o último filho do pai, se não for folha
    if (!child->is_leaf)
      child->children[0] = l_sibling->children[l_sibling->n_keys];

    node->keys[idx - 1] = l_sibling->keys[l_sibling->n_keys - 1];

    // Atualiza contadores
    child->n_keys++;
    l_sibling->n_keys--;

    return BTREE_SUCCESS;
  }
  // Caso 3a-dir: Empresta uma chave do irmão à direita
  else if (idx < node->n_keys && node->children[idx + 1]->n_keys >= t) {
    node_t *r_sibling = node->children[idx + 1];

    // Última chave do filho recebe chave do pai
    child->keys[child->n_keys] = node->keys[idx];

    // Se não for folha, também recebe o filho
    if (!child->is_leaf)
      child->children[child->n_keys + 1] = r_sibling->children[0];

    // Pai recebe primeira chave de r_sibling
    node->keys[idx] = r_sibling->keys[0];

    for (int i = 1; i < r_sibling->n_keys; i++)
      r_sibling->keys[i - 1] = r_sibling->keys[i];

    if (!r_sibling->is_leaf)
      for (int i = 1; i <= r_sibling->n_keys; i++)
        r_sibling->children[i - 1] = r_sibling->children[i];

    child->n_keys++;
    r_sibling->n_keys--;

    return BTREE_SUCCESS;
  }

  // Caso 3b: Mescla com um irmão
  if (idx < node->n_keys)
    return node_merge(node, idx, order);
  else
    return node_merge(node, idx - 1, order);
}

int node_remove(node_t *node, int key, size_t order) {
  if (!node)
    return BTREE_ERROR_NOT_FOUND;

  int idx = 0;
  while (idx < node->n_keys && key > node->keys[idx])
    idx++;

  // Casos 1 e 2
  if (idx < node->n_keys && key == node->keys[idx]) {
    // Caso 1: Nó folha -> simplesmente remove chave
    if (node->is_leaf)
      return node_remove_from_leaf(node, idx);
    // Casos 2
    else
      return node_remove_from_internal(node, idx, order);
  }

  // Se for nó folha, chave não está na árvore
  if (node->is_leaf)
    return BTREE_ERROR_NOT_FOUND;

  bool is_last = idx == node->n_keys;

  // Garantir que o filho onde a busca continua tenha pelo menos ⌈order/2⌉
  // chaves
  int result = node_ensure_min_keys(node, idx, order);
  if (result != BTREE_SUCCESS)
    return result;

  if (is_last && idx > node->n_keys)
    return node_remove(node->children[idx - 1], key, order);
  else
    return node_remove(node->children[idx], key, order);
}

void node_print(node_t *node) {
  if (!node) {
    printf("[ NULL ]");
    return;
  }

  printf("[ ");
  for (int i = 0; i < node->n_keys; i++) {
    printf("key%d: %d", i, node->keys[i]);
    if (i < node->n_keys - 1)
      printf(", ");
  }
  printf(" ]");
}

struct btree {
  size_t order;   // Ordem da árvore
  node_t *root;   // Ponteiro para o nó raiz
  size_t n_nodes; // Número de nós na árvore
};

btree_t *btree_create(size_t order) {
  if (order < 3)
    return NULL;

  btree_t *tree = malloc(sizeof(btree_t));
  if (!tree)
    return NULL;

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

int btree_insert(btree_t *tree, int key) {
  int result = node_insert(&tree->root, key, tree->order);
  if (result == BTREE_SUCCESS)
    tree->n_nodes++;

  return result;
}

int btree_remove(btree_t *tree, int key) {
  int result = node_remove(tree->root, key, tree->order);
  if (result == BTREE_SUCCESS)
    tree->n_nodes--;

  return result;
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
