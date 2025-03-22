#include "btree.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
  if (argc <= 2) {
    perror("Arguments missing");
    return EXIT_FAILURE;
  }

  FILE *input_fptr = fopen(argv[1], "r");

  FILE *output_fptr = fopen(argv[2], "w");

  size_t order;
  fscanf(input_fptr, "%ld", &order);

  btree_t *tree = btree_create(order, "database", "w+b");

  int op_num;
  fscanf(input_fptr, "%d\n", &op_num);

  for (int i = op_num; i > 0; i--) {
    char op = fgetc(input_fptr);

    if (op == 'I') {
      int key, value;
      fscanf(input_fptr, "%d, %d\n", &key, &value);

      btree_insert(tree, key, value);
    } else if (op == 'R') {
      int key;
      fscanf(input_fptr, "%d\n", &key);

      btree_remove(tree, key);
    } else if (op == 'B') {
      int key;
      fscanf(input_fptr, "%d\n", &key);

      int pos;
      node_t *node = btree_search(tree, key, &pos);
      if (node)
        fprintf(output_fptr, "O REGISTRO ESTA NA ARVORE!\n");
      else
        fprintf(output_fptr, "O REGISTRO NAO ESTA NA ARVORE!\n");
    } else {
      fprintf(output_fptr, "OPERACAO NAO SUPORTADA!\n");
    }
  }

  fprintf(output_fptr, "\n");

  int result = btree_print(tree, output_fptr);

  btree_destroy(tree);

  fclose(input_fptr);
  fclose(output_fptr);

  return 0;
}
