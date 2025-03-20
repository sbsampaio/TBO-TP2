#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "btree.h"

int main(int argc, char const *argv[]) {
  btree_t *tree = btree_create(4);

  srand(time(NULL));

  btree_print(tree);
  printf("\n");

  for (int i = 0; i < 10; i++)
    btree_insert(tree, rand() % 100 + 1);

  btree_print(tree);

  btree_destroy(tree);
  return 0;
}
