#include <stdio.h>
#include <stdlib.h>

#include "btree.h"

int main(int argc, char const *argv[]) {
  btree_t *tree = btree_create(10);

  for (int i = 0; i < 100; i++)
    btree_insert(tree, rand() % 100 + 1);

  btree_print(tree);

  btree_destroy(tree);
  return 0;
}
