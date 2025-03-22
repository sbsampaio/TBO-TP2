#include <stdio.h>
#include <stdlib.h>

#include "btree.h"

int main(int argc, char const *argv[]) {
  srand(05102003);
  btree_t *tree = btree_create(4);

  for (int i = 0; i < 20; i++)
    btree_insert(tree, rand() % 50 + 1);

  btree_print(tree);

  btree_remove(tree, 17);

  printf("\n");
  btree_print(tree);

  btree_destroy(tree);
  return 0;
}
