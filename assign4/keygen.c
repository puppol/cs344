#include <stdio.h>
#include <stdlib.h>
#include <time.h>


int main(int argc, char* argv[]) {
  srand(time(0));
  int i;
  char * key;

  if(argc < 2){
    perror("An incorrect number of arguments have been passed.\n");
    return 1;
  }

  int userNums = atoi(argv[1]);
  key = malloc(sizeof(char) * (userNums + 2));
  for(i = 0; i < userNums; i++){
    key[i] = 'A' + (rand() % 27);
    if(key[i] == ('[')) key[i] = ' ';
  }
  key[userNums] = '\n';
  key[userNums + 1] = '\0';

  printf(key);

  return 0;
}
