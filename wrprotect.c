#include "types.h"
#include "stat.h"
#include "user.h"

void
writemem()
{
  // get memory
  char *mem = sbrk(100);
  // write into mem
  *((int *)mem) = 10;
  // read mem
  printf(1, "read mem %d, should success\n", *((int *)mem));

  // protect first int size memory
  wrprotect(mem, sizeof(int));
  // try to read again
  printf(1, "after protect, read mem %d, should success\n", *((int *)mem));
  // try to write
  *((int *)mem) = 2;

  // try to read again
  printf(1, "after protect and write again, read mem %d, should not be 2\n", *((int *)mem));

}

int
main(int argc, char *argv[])
{
  writemem();
  exit();
}
