#ifndef BTREE_H
#define BTREE_H

#include <stdlib.h>

// Códigos de erro para retorno das funções
#define BTREE_SUCCESS 0
#define BTREE_ERROR_ALLOC -1
#define BTREE_ERROR_NOT_FOUND -2
#define BTREE_ERROR_DUPLICATE -3
#define BTREE_ERROR_INVALID_PARAM -4

typedef struct node node_t;

typedef struct btree btree_t;

/**
 * Imprime um nó
 *
 * @param node Nó a ser impresso
 */
void node_print(node_t* node);

/**
 * Cria uma nova árvore B e aloca memória para ela
 *
 * @param order Ordem da árvore (mínimo 3)
 *
 * @return Ponteiro para a nova árvore ou NULL em caso de erro
 */
btree_t* btree_create(size_t order);

/**
 * Destrói a árvore B e libera a memória alocada
 *
 * @param tree Ponteiro para uma árvore B alocada
 */
void btree_destroy(btree_t* tree);

/**
 * Função que busca uma chave na árvore
 *
 * @param tree Ponteiro para árvore B
 * @param key Chave a ser buscada
 * @param pos Ponteiro para pos que irá guardar o índice da chave encontrada
 * ou -1 se não encontrada
 *
 * @return Ponteiro para o nó encontrado ou NULL caso contrário
 */
node_t* btree_search(btree_t* tree, int key, int* pos);

/**
 * Função para inserir uma chave na árvore
 *
 * @param tree Ponteiro para árvore B
 * @param key Chave a ser inserida
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int btree_insert(btree_t* tree, int key);

/**
 * Remove uma chave da árvore
 *
 * @param tree Ponteiro para árvore B
 * @param key Chave a ser removida
 *
 * @return BTREE_SUCCESS em caso de sucesso ou código de erro
 */
int btree_remove(btree_t* tree, int key);

/**
 * Imprime a árvore na saída padrão
 *
 * @param tree Ponteiro para árvore B
 */
void btree_print(btree_t* tree);

#endif // !BTREE_H
