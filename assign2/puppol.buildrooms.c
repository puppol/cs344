#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>



char roomNames[10][9] =
{
  "aa",
  "bb",
  "cc",
  "dd",
  "ee",
  "ff",
  "gg",
  "hh",
  "ii",
  "jj"
};

struct Room {
  char roomName [9];
  int numConnections;
  struct Room * outboundConnections[6];
};

struct Room rooms[7];



// Returns true if all rooms have 3 to 6 outbound connections, false otherwise
int IsGraphFull() {
  int i;
  for (i = 0; i < 7; i++) {
    if (rooms[i].numConnections < 3 || rooms[i].numConnections > 6) {
      return 0;
    }
  }
  return 1;
}

// Returns a random Room, does NOT validate if connection can be added
struct Room* GetRandomRoom()
{
  return &rooms[rand() % 7];
}


// Adds a random, valid outbound connection from a Room to another Room
void AddRandomConnection()
{
  struct Room* A;  // Maybe a struct, maybe global arrays of ints
  struct Room* B;

  while(1)
  {
    A = GetRandomRoom();

    if (CanAddConnectionFrom(A) == 1)
      break;
  }

  do
  {
    B = GetRandomRoom();
  }
  while(!CanAddConnectionFrom(B) || IsSameRoom(A, B) || ConnectionAlreadyExists(A, B));

  ConnectRoom(A, B);  // TODO: Add this connection to the real variables,
  ConnectRoom(B, A);  //  because this A and B will be destroyed when this function terminates
}




// Returns true if a connection can be added from Room x (< 6 outbound connections), false otherwise
int CanAddConnectionFrom(struct Room * x)
{
  if(x->numConnections < 6)
    return 1;
  return 0;
}
// Returns true if a connection from Room x to Room y already exists, false otherwise
int ConnectionAlreadyExists(struct Room * x, struct Room * y)
{
  int i;
  for(i = 0; i < x->numConnections; i++){
    if(x->outboundConnections[i] == y){
      return 1;
    }
  }
  return 0;
}

// Connects Rooms x and y together, does not check if this connection is valid
void ConnectRoom(struct Room * x, struct Room * y)
{
  x->outboundConnections[x->numConnections] = y;

  x->numConnections += 1;
}

// Returns true if Rooms x and y are the same Room, false otherwise
int IsSameRoom(struct Room * x, struct Room * y)
{
  if(x == y) {
    return 1;
  }
  return 0;
}


int isNumInArr(int num, int * arr) {
  int i;
  for (i=0; i < 7; i++) {
    if (arr[i] == num)
      return 1;
    }
  return 0;
}

void buildRoomsArray(){
  int * arr = malloc(7 * sizeof(int));
  int num;
  int i;
  for (i = 0; i < 7; i++){
    do {
      num = rand() % 10;
    } while( isNumInArr(num, arr));
    arr[i] = num;
  }
  for (i = 0; i < 7; i++) {
    strcpy(rooms[i].roomName, roomNames[arr[i]]);
    //printf("%s", rooms[i].roomName);
  }
  free(arr);
}


void buildFiles(int pid){
  char command[50];
  char directory[20];
  sprintf(directory,"puppol.rooms.%d/",pid);
  sprintf(command,"mkdir %s",directory);
  system(command);


  int i;
  int j;
  for (i = 0; i < 7; i++){
    char file[40];
    strcpy(file, directory);
    strcat(file,rooms[i].roomName);
    strcat(file,"_room.txt");

    FILE *fptr;
    fptr = fopen(file,"a");
    printf("Creating file: %s\n", file);
    if(fptr == NULL){
      printf("Error!");
      exit(1);
   }
   fprintf(fptr,"ROOM NAME: %s\n",rooms[i].roomName);
    for (j = 0; j < rooms[i].numConnections; j++){
      fprintf(fptr,"CONNECTION %d: %s\n", j+1, rooms[i].outboundConnections[j]->roomName);
    }

    switch(i) {
      case 0:
        fprintf(fptr, "ROOM TYPE: START_ROOM\n");
        break;
      case 1:
        fprintf(fptr, "ROOM TYPE: END_ROOM\n");
        break;
      default:
        fprintf(fptr, "ROOM TYPE: MID_ROOM\n");
        break;
    }
   fclose(fptr);
   memset(file, '\0', sizeof(file));
  }
}



int main(void) {
  srand(time(NULL));

  pid_t pid;
  pid = getpid();
  //Build directory
  int i;

  buildRoomsArray();


  //Create all connections in graph
  while (!IsGraphFull())
  {
    AddRandomConnection();
  }

  buildFiles(pid);

}
