#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "btree.h"

struct node {
  size_t n_keys; // Quantidade de chaves armazenadas
  int *keys;     // Array de chaves
  int *values;   // Array de registros

  size_t bin_pos; // Posição no arquivo binário
  int *children;  // Array offsets para leitura dos filhos em arquivo binário

  bool is_leaf; // Flag indicando se um nó é folha
};

size_t get_next_bin_pos(FILE *fp) {
  size_t n_nodes;
  fseek(fp, 0, SEEK_SET);
  fread(&n_nodes, sizeof(size_t), 1, fp);
  return n_nodes;
}

void update_node_count(FILE *fp) {
  size_t n_nodes;
  fseek(fp, 0, SEEK_SET);
  fread(&n_nodes, sizeof(size_t), 1, fp);
  n_nodes++;
  fseek(fp, 0, SEEK_SET);
  fwrite(&n_nodes, sizeof(size_t), 1, fp);
}

size_t calculate_offset(size_t bin_pos, size_t order) {
  if (order < 3)
    return (size_t)-1;

  size_t static_var = sizeof(size_t) + sizeof(bool) + sizeof(size_t);
  size_t key_size = sizeof(int) * (order - 1);
  size_t value_size = sizeof(int) * (order - 1);
  size_t child_size = sizeof(int) * order;

  return bin_pos * (static_var + key_size + value_size + child_size);
}

node_t *node_create(bool is_leaf, size_t order, size_t bin_pos);

void node_free(node_t *node);

node_t *disk_read(FILE *fp, size_t order, size_t file_pos) {
  if (!fp || order < 3)
    return NULL;

  long offset = calculate_offset(file_pos, order);
  if (offset < 0 || fseek(fp, offset, SEEK_SET) != 0)
    return NULL;

  size_t n_keys, bin_pos;
  bool is_leaf;

  if (fread(&n_keys, sizeof(size_t), 1, fp) != 1 ||
      fread(&is_leaf, sizeof(bool), 1, fp) != 1 ||
      fread(&bin_pos, sizeof(size_t), 1, fp) != 1)
    return NULL;

  node_t *r_node = node_create(is_leaf, order, file_pos);
  if (!r_node)
    return NULL;

  if (fread(r_node->keys, sizeof(int), order - 1, fp) != order - 1 ||
      fread(r_node->values, sizeof(int), order - 1, fp) != order - 1 ||
      fread(r_node->children, sizeof(int), order, fp) != order) {
    node_free(r_node);
    return NULL;
  }

  return r_node;
}

int disk_write(FILE *fp, node_t *node, size_t order) {
  if (!fp || !node || order < 3)
    return BTREE_ERROR_INVALID_PARAM;

  long offset = calculate_offset(node->bin_pos, order);
  if (offset < 0 || fseek(fp, offset, SEEK_SET) != 0)
    return BTREE_ERROR_IO;

  if (fwrite(&node->n_keys, sizeof(size_t), 1, fp) != 1 ||
      fwrite(&node->is_leaf, sizeof(bool), 1, fp) != 1 ||
      fwrite(&node->bin_pos, sizeof(size_t), 1, fp) != 1)
    return BTREE_ERROR_IO;

  if (fwrite(node->keys, sizeof(int), order - 1, fp) != order - 1 ||
      fwrite(node->values, sizeof(int), order - 1, fp) != order - 1 ||
      fwrite(node->children, sizeof(int), order, fp) != order)
    return BTREE_ERROR_IO;

  // Força atualização do buffer
  fflush(fp);

  return (int)node->bin_pos;
}

/**
 * Destrói recursivamente o nó e seus filhos da memória e do arquivo binário
 *
 * @param node Ponteiro válido para o nó a ser destruído
 * @param order Ordem da árvore
 * @param fp Ponteiro para o arquivo aberto
 */
void node_destroy(node_t *node, size_t order, FILE *fp) {
  if (!node)
    return;

  if (node->children && !node->is_leaf) {
    for (int i = 0; i <= node->n_keys; i++)
      if (node->children[i] != -1) {
        node_t *child = disk_read(fp, order, node->children[i]);
        if (child) {
          int child_pos = node->children[i];
          node->children[i] = -1;
          disk_write(fp, node, order);

          node_destroy(child, order, fp);
        }
      }
  }

  if (node->children)
    free(node->children);

  if (node->keys)
    free(node->keys);

  if (node->values)
    free(node->values);

  free(node);
}

/**
 * Libera memória alocada pelo nó
 *
 * @param node Ponteiro para o nó a ser liberado
 */
void node_free(node_t *node) {
  if (!node)
    return;

  if (node->children)
    free(node->children);

  if (node->keys)
    free(node->keys);

  if (node->values)
    free(node->values);

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
node_t *node_create(bool is_leaf, size_t order, size_t bin_pos) {
  if (order < 3)
    return NULL;

  node_t *new_node = malloc(sizeof(node_t));
  if (!new_node)
    return NULL;

  new_node->n_keys = 0;
  new_node->is_leaf = is_leaf;
  new_node->bin_pos = bin_pos;

  new_node->keys = malloc((order - 1) * sizeof(int));
  if (!new_node->keys) {
    free(new_node);
    return NULL;
  }

  for (size_t i = 0; i < order - 1; i++)
    new_node->keys[i] = -1;

  new_node->values = malloc((order - 1) * sizeof(int));
  if (!new_node->values) {
    free(new_node->keys);
    free(new_node);
    return NULL;
  }

  for (size_t i = 0; i < order - 1; i++)
    new_node->values[i] = -1;

  new_node->children = malloc(order * sizeof(int));
  if (!new_node->children) {
    free(new_node->keys);
    free(new_node->values);
    free(new_node);
    return NULL;
  }

  for (size_t i = 0; i < order; i++)
    new_node->children[i] = -1;

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
node_t *node_search(node_t *node, int key, int *pos, FILE *fp, size_t order) {
  if (!node || !pos || !fp)
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

  if (node->children[i] == -1)
    return NULL;

  node_t *child = disk_read(fp, order, node->children[i]);
  if (!child)
    return NULL;

  node_t *result = node_search(child, key, pos, fp, order);

  if (child != result)
    node_free(child);

  return result;
}

/**
 * Divide um nó filho quando está cheio
 *
 * @param parent Nó pai
 * @param idx Índice do filho a ser dividido
 * @param order Ordem da árvore
 * @param child Nó filho
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_split_child(node_t *parent, int idx, size_t order, node_t *child,
                     FILE *fp) {
  if (!parent || !child || !fp || idx < 0 || idx > parent->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  // Determina próxima posição livre no arquivo
  size_t new_node_pos = get_next_bin_pos(fp);

  node_t *new_node = node_create(child->is_leaf, order, new_node_pos);

  if (!new_node)
    return BTREE_ERROR_ALLOC;

  int t = ceil(order / 2.0);
  new_node->n_keys = child->n_keys - t; // Metade das chaves vai para o novo nó

  // Copia as chaves do nó filho para novo nó
  for (int i = 0; i < new_node->n_keys; i++) {
    new_node->keys[i] = child->keys[t + i];
    new_node->values[i] = child->values[t + i];

    // Limpar nó original
    child->keys[t + i] = -1;
    child->values[t + i] = -1;
  }

  // Se não for folha, copia os ponteiros para filhos também
  if (!child->is_leaf)
    for (int i = 0; i <= new_node->n_keys; i++) {
      new_node->children[i] = child->children[t + i];

      // Limpar nó original
      child->children[t + i] = -1;
    }

  // Atualiza o número de chaves no filho
  child->n_keys = t - 1;

  // Move os ponteiros para filhos no pai para abrir espaço
  for (int i = parent->n_keys; i > idx; i--)
    parent->children[i + 1] = parent->children[i];

  // Liga o novo nó ao pai
  parent->children[idx + 1] = new_node->bin_pos;

  // Move as chaves no pai para abrir espaço
  for (int i = parent->n_keys - 1; i >= idx; i--) {
    parent->keys[i + 1] = parent->keys[i];
    parent->values[i + 1] = parent->values[i];
  }

  // Insere a chave do meio do filho no pai
  parent->keys[idx] = child->keys[t - 1];
  parent->values[idx] = child->values[t - 1];
  parent->n_keys++;

  child->keys[t - 1] = -1;
  child->values[t - 1] = -1;

  node_print(child, stdout);
  printf("Child position: %ld\n", child->bin_pos);
  printf("\n");

  int write_result;
  write_result = disk_write(fp, child, order);
  if (write_result < 0) {
    node_free(new_node);
    return BTREE_ERROR_IO;
  }

  node_print(new_node, stdout);
  printf("new_node position: %ld\n", new_node->bin_pos);
  printf("\n");

  write_result = disk_write(fp, new_node, order);
  if (write_result < 0) {
    node_free(new_node);
    return BTREE_ERROR_IO;
  }

  node_print(parent, stdout);
  printf("parent position: %ld\n", parent->bin_pos);
  printf("\n");
  write_result = disk_write(fp, parent, order);
  if (write_result < 0) {
    node_free(new_node);
    return BTREE_ERROR_IO;
  }

  node_free(new_node);
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
int node_insert_non_full(node_t *node, int key, int value, size_t order,
                         FILE *fp) {
  if (!node || !fp)
    return BTREE_ERROR_INVALID_PARAM;

  int i = node->n_keys - 1;

  if (node->is_leaf) {
    // Encontra a posição correta e insere a chave
    while (i >= 0 && key < node->keys[i]) {
      node->keys[i + 1] = node->keys[i];
      node->values[i + 1] = node->values[i];
      i--;
    }

    node->keys[i + 1] = key;
    node->values[i + 1] = value;
    node->n_keys++;

    if (disk_write(fp, node, order) < 0)
      return BTREE_ERROR_IO;

    return BTREE_SUCCESS;
  } else {
    // Encontra o filho onde a chave deve ser inserida
    while (i >= 0 && key < node->keys[i])
      i--;

    i++;

    // Carrega filho do disco
    node_t *child = disk_read(fp, order, node->children[i]);
    if (!child)
      return BTREE_ERROR_IO;

    // Divide o filho, se cheio
    if (child->n_keys == order - 1) {
      int result = node_split_child(node, i, order, child, fp);
      if (result != BTREE_SUCCESS) {
        node_free(child);
        return result;
      }

      node_t *updated_node = disk_read(fp, order, node->bin_pos);
      if (!updated_node)
        return BTREE_ERROR_IO;

      *node = *updated_node;
      node_free(updated_node);

      // Decide qual dos filhos vai conter a chave
      if (node->keys[i] < key) {
        i++;
        node_free(child);
        child = disk_read(fp, order, node->children[i]);
        if (!child)
          return BTREE_ERROR_IO;
      }
    }

    // Insere a chave recursivamente no filho apropriado
    int result = node_insert_non_full(child, key, value, order, fp);

    node_free(child);
    return result;
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
int node_insert(node_t **root, int key, int value, size_t order, FILE *fp) {
  if (!root || !fp)
    return BTREE_ERROR_INVALID_PARAM;

  // Verifica se chave já existe
  int pos;
  if (*root) {
    node_t *found = node_search(*root, key, &pos, fp, order);
    if (found) {
      found->values[pos] = value;
      int result = disk_write(fp, found, order);
      if (found != *root)
        node_free(found);
      return (result < 0) ? BTREE_ERROR_IO : BTREE_SUCCESS;
    }
  }

  // Se a raiz for NULL, cria uma nova raiz
  if (!(*root)) {
    // Determina posição do nó no arquivo
    fseek(fp, 0, SEEK_END);
    size_t root_pos = ftell(fp) / calculate_offset(1, order);

    *root = node_create(true, order, root_pos);
    if (!(*root))
      return BTREE_ERROR_ALLOC;

    (*root)->keys[0] = key;
    (*root)->values[0] = value;
    (*root)->n_keys = 1;

    if (disk_write(fp, *root, order) < 0) {
      node_free(*root);
      *root = NULL;
      return BTREE_ERROR_IO;
    }

    return BTREE_SUCCESS;
  }

  // Se raiz estiver cheia, cria nova raiz
  if ((*root)->n_keys == order - 1) {
    // Determina posição arquivo binário
    fseek(fp, 0, SEEK_END);
    size_t new_root_pos = ftell(fp) / calculate_offset(1, order);

    node_t *new_root = node_create(false, order, new_root_pos);
    if (!new_root)
      return BTREE_ERROR_ALLOC;

    new_root->children[0] = (*root)->bin_pos;

    int result = node_split_child(new_root, 0, order, *root, fp);
    if (result != BTREE_SUCCESS) {
      node_free(new_root);
      return result;
    }

    node_free(*root);
    *root = new_root;

    return node_insert_non_full(*root, key, value, order, fp);
  }

  return node_insert_non_full(*root, key, value, order, fp);
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
int node_predecessor(node_t *node, int idx, int *pred, FILE *fp, size_t order) {
  if (!node || !pred || !fp || idx < 0 || idx >= node->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  if (node->is_leaf)
    return BTREE_ERROR_INVALID_PARAM;

  // Vai para o filho à esquerda da chave
  node_t *curr = disk_read(fp, order, node->children[idx]);
  if (!curr)
    return BTREE_ERROR_IO;

  // Desce até o elemento mais à direita (maior valor)
  while (!curr->is_leaf) {
    if (curr->children[curr->n_keys] == -1) {
      node_free(curr);
      return BTREE_ERROR_INVALID_PARAM;
    }

    node_t *next = disk_read(fp, order, curr->children[curr->n_keys]);
    node_free(curr);
    if (!next)
      return BTREE_ERROR_IO;
    curr = next;
  }

  if (curr->n_keys <= 0) {
    node_free(curr);
    return BTREE_ERROR_INVALID_PARAM;
  }

  *pred = curr->keys[curr->n_keys - 1];

  node_free(curr);
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
int node_successor(node_t *node, int idx, int *succ, FILE *fp, size_t order) {
  if (!node || !succ || !fp || idx < 0 || idx >= node->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  if (node->is_leaf)
    return BTREE_ERROR_INVALID_PARAM;

  // Vai para o filho à direita da chave
  node_t *curr = disk_read(fp, order, node->children[idx + 1]);
  if (!curr)
    return BTREE_ERROR_IO;

  // Desce até o elemento mais à esquerda (menor valor)
  while (!curr->is_leaf) {
    if (curr->children[0] == -1) {
      node_free(curr);
      return BTREE_ERROR_INVALID_PARAM;
    }

    node_t *next = disk_read(fp, order, curr->children[0]);
    node_free(curr);
    if (!next)
      return BTREE_ERROR_IO;
    curr = next;
  }

  if (curr->n_keys <= 0) {
    node_free(curr);
    return BTREE_ERROR_INVALID_PARAM;
  }

  *succ = curr->keys[0];

  node_free(curr);
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
int node_merge(node_t *parent, int idx, size_t order, FILE *fp) {
  if (!parent || !fp || idx < 0 || idx >= parent->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  node_t *l_child = disk_read(fp, order, parent->children[idx]);
  if (!l_child)
    return BTREE_ERROR_IO;

  node_t *r_child = disk_read(fp, order, parent->children[idx + 1]);
  if (!r_child) {
    node_free(l_child);
    return BTREE_ERROR_IO;
  }

  int t = ceil(order / 2.0);

  // Move a chave do pai para o filho à esquerda
  l_child->keys[l_child->n_keys] = parent->keys[idx];
  l_child->values[l_child->n_keys] = parent->values[idx];

  // Move todas as chaves do filho à direita para o filho à esquerda
  for (int i = 0; i < r_child->n_keys; i++) {
    l_child->keys[l_child->n_keys + 1 + i] = r_child->keys[i];
    l_child->values[l_child->n_keys + 1 + i] = r_child->values[i];
  }

  // Move também ponteiros para filhos, se não for folha
  if (!l_child->is_leaf)
    for (int i = 0; i <= r_child->n_keys; i++)
      l_child->children[l_child->n_keys + 1 + i] = r_child->children[i];

  // Atualiza número de chaves filho à esquerda
  l_child->n_keys += r_child->n_keys + 1;

  // Remove chave do pai e ajusta ponteiros
  for (int i = idx; i < parent->n_keys - 1; i++) {
    parent->keys[i] = parent->keys[i + 1];
    parent->values[i] = parent->values[i + 1];
  }

  for (int i = idx + 1; i < parent->n_keys; i++)
    parent->children[i] = parent->children[i + 1];

  parent->keys[parent->n_keys - 1] = -1;
  parent->values[parent->n_keys - 1] = -1;
  parent->children[parent->n_keys] = -1;
  parent->n_keys--;

  int r_child_pos = r_child->bin_pos;

  int result = disk_write(fp, l_child, order);
  if (result < 0) {
    node_free(l_child);
    node_free(r_child);
    return BTREE_ERROR_IO;
  }

  result = disk_write(fp, parent, order);
  if (result < 0) {
    node_free(l_child);
    node_free(r_child);
    return BTREE_ERROR_IO;
  }

  node_free(l_child);
  node_free(r_child);

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
int node_remove_from_leaf(node_t *node, int idx, FILE *fp, size_t order) {
  if (!node || !fp || idx < 0 || idx >= node->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  // Move todas as chaves à frente de node->keys[idx] uma posição para trás
  for (int i = idx; i < node->n_keys - 1; i++) {
    node->keys[i] = node->keys[i + 1];
    node->values[i] = node->values[i + 1];
  }

  node->keys[node->n_keys - 1] = -1;
  node->values[node->n_keys - 1] = -1;
  node->n_keys--;

  if (disk_write(fp, node, order) < 0)
    return BTREE_ERROR_IO;

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
int node_remove(node_t *node, int key, size_t order, FILE *fp);

/**
 * Remove uma chave de um nó interno
 *
 * @param node Nó de onde remover
 * @param idx Índice da chave
 * @param order Ordem da chave
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int node_remove_from_internal(node_t *node, int idx, size_t order, FILE *fp) {
  if (!node || !fp || idx < 0 || idx >= node->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  int key = node->keys[idx];
  int t = ceil(order / 2.0);

  // Carrega filho esquerdo
  node_t *left_child = disk_read(fp, order, node->children[idx]);
  if (!left_child)
    return BTREE_ERROR_IO;

  // Caso 2a: O filho à esquerda tem pelo menos t chaves
  if (left_child->n_keys >= t) {
    int pred;
    int result = node_predecessor(node, idx, &pred, fp, order);
    node_free(left_child);

    if (result != BTREE_SUCCESS)
      return result;

    node->keys[idx] = pred;
    node->values[idx] = pred;

    // Atualiza nó na memória
    result = disk_write(fp, node, order);
    if (result < 0)
      return BTREE_ERROR_IO;

    node_t *child = disk_read(fp, order, node->children[idx]);
    if (!child)
      return BTREE_ERROR_IO;

    result = node_remove(child, pred, order, fp);
    node_free(child);
    return result;
  }

  node_free(left_child);

  node_t *right_child = disk_read(fp, order, node->children[idx + 1]);
  if (!right_child)
    return BTREE_ERROR_IO;

  // Caso 2b: O filho à direita tem pelo menos t chaves
  if (right_child->n_keys >= t) {
    int succ;
    int result = node_successor(node, idx, &succ, fp, order);
    node_free(right_child);

    if (result != BTREE_SUCCESS)
      return result;

    node->keys[idx] = succ;
    node->values[idx] = succ;

    result = disk_write(fp, node, order);
    if (result < 0)
      return BTREE_ERROR_IO;

    node_t *child = disk_read(fp, order, node->children[idx + 1]);
    if (!child)
      return BTREE_ERROR_IO;

    result = node_remove(child, succ, order, fp);
    node_free(child);
    return result;
  }

  node_free(right_child);

  // Caso 2c: Ambos os filhos têm menos de t chaves

  // Mescla os filhos e depois remove a chave do filho mesclado
  int result = node_merge(node, idx, order, fp);
  if (result != BTREE_SUCCESS)
    return result;

  node_t *child = disk_read(fp, order, node->children[idx]);
  if (!child)
    return BTREE_ERROR_IO;

  result = node_remove(child, key, order, fp);
  node_free(child);
  return result;
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
int node_ensure_min_keys(node_t *node, int idx, size_t order, FILE *fp) {
  if (!node || !fp || idx < 0 || idx > node->n_keys)
    return BTREE_ERROR_INVALID_PARAM;

  node_t *child = disk_read(fp, order, node->children[idx]);
  if (!child)
    return BTREE_ERROR_IO;

  int t = ceil(order / 2.0);

  if (child->n_keys >= t) {
    node_free(child);
    return BTREE_SUCCESS;
  }

  // Caso 3a-esq: Empresta uma chave do irmão à esquerda
  if (idx > 0) {
    node_t *l_sibling = disk_read(fp, order, node->children[idx - 1]);
    if (!l_sibling) {
      node_free(child);
      return BTREE_ERROR_IO;
    }

    if (l_sibling->n_keys >= t) {
      // Move todas as chaves uma posição para a direita
      for (int i = child->n_keys - 1; i >= 0; i--) {
        child->keys[i + 1] = child->keys[i];
        child->values[i + 1] = child->values[i];
      }

      if (!child->is_leaf)
        for (int i = child->n_keys; i >= 0; i--)
          child->children[i + 1] = child->children[i];

      child->keys[0] = node->keys[idx - 1];
      child->values[0] = node->values[idx - 1];

      // O primeiro filho recebe o último filho do pai, se não for folha
      if (!child->is_leaf)
        child->children[0] = l_sibling->children[l_sibling->n_keys];

      node->keys[idx - 1] = l_sibling->keys[l_sibling->n_keys - 1];
      node->values[idx - 1] = l_sibling->values[l_sibling->n_keys - 1];

      // Limpa vetores no irmão
      l_sibling->keys[l_sibling->n_keys - 1] = -1;
      l_sibling->values[l_sibling->n_keys - 1] = -1;
      if (!l_sibling->is_leaf)
        l_sibling->children[l_sibling->n_keys] = -1;

      child->n_keys++;
      l_sibling->n_keys--;

      int result = disk_write(fp, child, order);
      if (result < 0) {
        node_free(child);
        node_free(l_sibling);
        return BTREE_ERROR_IO;
      }

      result = disk_write(fp, l_sibling, order);
      if (result < 0) {
        node_free(child);
        node_free(l_sibling);
        return BTREE_ERROR_IO;
      }

      result = disk_write(fp, node, order);
      if (result < 0) {
        node_free(child);
        node_free(l_sibling);
        return BTREE_ERROR_IO;
      }

      node_free(child);
      node_free(l_sibling);
      return BTREE_SUCCESS;
    }

    node_free(l_sibling);
  }

  // Caso 3a-dir: Empresta uma chave do irmão à direita
  if (idx < node->n_keys) {
    // Carrega o irmão direito
    node_t *r_sibling = disk_read(fp, order, node->children[idx + 1]);
    if (!r_sibling) {
      node_free(child);
      return BTREE_ERROR_IO;
    }

    if (r_sibling->n_keys >= t) {
      // Última chave do filho recebe chave do pai
      child->keys[child->n_keys] = node->keys[idx];
      child->values[child->n_keys] = node->values[idx];

      // Se não for folha, também recebe o filho
      if (!child->is_leaf)
        child->children[child->n_keys + 1] = r_sibling->children[0];

      // Pai recebe primeira chave de r_sibling
      node->keys[idx] = r_sibling->keys[0];
      node->values[idx] = r_sibling->values[0];

      // Move as chaves no irmão
      for (int i = 1; i < r_sibling->n_keys; i++) {
        r_sibling->keys[i - 1] = r_sibling->keys[i];
        r_sibling->values[i - 1] = r_sibling->values[i];
      }

      // Move também ponteiros para filhos, se não for folha
      if (!r_sibling->is_leaf)
        for (int i = 1; i <= r_sibling->n_keys; i++)
          r_sibling->children[i - 1] = r_sibling->children[i];

      // Limpa valores no irmão
      r_sibling->keys[r_sibling->n_keys - 1] = -1;
      r_sibling->values[r_sibling->n_keys - 1] = -1;
      if (!r_sibling->is_leaf)
        r_sibling->children[r_sibling->n_keys] = -1;

      child->n_keys++;
      r_sibling->n_keys--;

      // Escreve as alterações em disco
      int result = disk_write(fp, child, order);
      if (result < 0) {
        node_free(child);
        node_free(r_sibling);
        return BTREE_ERROR_IO;
      }

      result = disk_write(fp, r_sibling, order);
      if (result < 0) {
        node_free(child);
        node_free(r_sibling);
        return BTREE_ERROR_IO;
      }

      result = disk_write(fp, node, order);
      if (result < 0) {
        node_free(child);
        node_free(r_sibling);
        return BTREE_ERROR_IO;
      }

      node_free(child);
      node_free(r_sibling);
      return BTREE_SUCCESS;
    }

    node_free(r_sibling);
  }

  // Caso 3b: Mescla com um irmão
  node_free(child);

  if (idx < node->n_keys)
    return node_merge(node, idx, order, fp);
  else
    return node_merge(node, idx - 1, order, fp);
}

int node_remove(node_t *node, int key, size_t order, FILE *fp) {
  if (!node || !fp)
    return BTREE_ERROR_NOT_FOUND;

  int idx = 0;
  while (idx < node->n_keys && key > node->keys[idx])
    idx++;

  // Casos 1 e 2
  if (idx < node->n_keys && key == node->keys[idx]) {
    // Caso 1: Nó folha -> simplesmente remove chave
    if (node->is_leaf)
      return node_remove_from_leaf(node, idx, fp, order);
    // Casos 2
    else
      return node_remove_from_internal(node, idx, order, fp);
  }

  // Se for nó folha, chave não está na árvore
  if (node->is_leaf)
    return BTREE_ERROR_NOT_FOUND;

  bool is_last = idx == node->n_keys;

  // Garantir que o filho onde a busca continua tenha pelo menos ⌈order/2⌉
  // chaves
  int result = node_ensure_min_keys(node, idx, order, fp);
  if (result != BTREE_SUCCESS)
    return result;

  node_t *updated_node = disk_read(fp, order, node->bin_pos);
  if (!updated_node)
    return BTREE_ERROR_IO;

  if (is_last && idx > updated_node->n_keys)
    idx--;

  node_t *child = disk_read(fp, order, updated_node->children[idx]);
  if (!child) {
    node_free(updated_node);
    return BTREE_ERROR_IO;
  }

  result = node_remove(child, key, order, fp);

  node_free(child);
  node_free(updated_node);

  return result;
}

void node_print(node_t *node, FILE *output_fptr) {
  if (!node) {
    fprintf(output_fptr, "[ NULL ]");
    return;
  }

  fprintf(output_fptr, "[");
  for (int i = 0; i < node->n_keys; i++)
    fprintf(output_fptr, "key%d: %d, ", i, node->keys[i]);

  fprintf(output_fptr, " ]");
}

struct btree {
  size_t order;   // Ordem da árvore
  node_t *root;   // Ponteiro para o nó raiz
  size_t n_nodes; // Número de nós na árvore
  FILE *fp;       // Ponteiro para o arquivo
};

btree_t *btree_create(size_t order, const char *filename, const char *mode) {
  if (order < 3 || !filename || !mode)
    return NULL;

  btree_t *tree = malloc(sizeof(btree_t));
  if (!tree)
    return NULL;

  tree->fp = fopen(filename, mode);
  if (!tree->fp) {
    free(tree);
    return NULL;
  }

  tree->order = order;
  tree->root = NULL;
  tree->n_nodes = 0;

  return tree;
}

void btree_destroy(btree_t *tree) {
  if (!tree)
    return;

  if (tree->root)
    node_destroy(tree->root, tree->order, tree->fp);

  if (tree->fp)
    fclose(tree->fp);

  free(tree);
}

node_t *btree_search(btree_t *tree, int key, int *pos) {
  return node_search(tree->root, key, pos, tree->fp, tree->order);
}

int btree_insert(btree_t *tree, int key, int value) {
  int result = node_insert(&tree->root, key, value, tree->order, tree->fp);
  if (result == BTREE_SUCCESS) {
    tree->n_nodes++;
    update_node_count(tree->fp);
  }

  return result;
}

int btree_remove(btree_t *tree, int key) {
  int result = node_remove(tree->root, key, tree->order, tree->fp);
  if (result == BTREE_SUCCESS)
    tree->n_nodes--;

  return result;
}

void enqueue(node_t **queue, node_t *node, int *rear) {
  queue[(*rear)++] = node;
}

int btree_print(btree_t *tree, FILE *output_fptr) {
  if (!tree || !tree->fp)
    return BTREE_ERROR_INVALID_PARAM;

  fprintf(output_fptr, "-- ARVORE B\n");

  if (!tree->root)
    return BTREE_ERROR_INVALID_PARAM;

  node_print(tree->root, output_fptr);
  fprintf(output_fptr, "\n");

  node_t **queue = calloc(tree->n_nodes, sizeof(node_t *));
  if (!queue)
    return BTREE_ERROR_ALLOC;
  int front = 0, rear = 0;
  int level = 1;
  int nodes_curr_lvl = 0;
  int nodes_nxt_lvl = 0;

  if (!tree->root->is_leaf) {
    for (int i = 0; i <= tree->root->n_keys; i++) {
      node_t *child = disk_read(tree->fp, tree->order, tree->root->children[i]);
      if (!child) {
        free(queue);
        return BTREE_ERROR_IO;
      }

      enqueue(queue, child, &rear);
      nodes_nxt_lvl++;
    }
  }

  while (front < rear) {
    if (nodes_curr_lvl == 0) {
      nodes_curr_lvl = nodes_nxt_lvl;
      nodes_nxt_lvl = 0;
      level++;
    }
    node_t *curr = queue[front++];
    nodes_curr_lvl--;

    node_print(curr, output_fptr);

    if (!curr->is_leaf) {
      for (int i = 0; i <= curr->n_keys; i++) {
        node_t *child = disk_read(tree->fp, tree->order, curr->children[i]);
        if (!child) {
          free(queue);
          return BTREE_ERROR_IO;
        }

        enqueue(queue, child, &rear);
        nodes_nxt_lvl++;
      }
    }

    if (nodes_curr_lvl <= 0)
      fprintf(output_fptr, "\n");
  }

  free(queue);

  return BTREE_SUCCESS;
}
