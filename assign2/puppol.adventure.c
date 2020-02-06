#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>


struct Room {
  char roomName[9];
  char roomType[12];
  char outboundConnections[6][3];
  int numConnections;
};

struct Room rooms[7];

char **roomPath;
int numPaths;

pthread_mutex_t MUTEX;
pthread_t thread;


void rebuildRoomsArray(){
  //Read most recent room files
  int i;
  int j;
  struct dirent *de;
  struct dirent *mostRecentDir;
  char * mostRecentDirStr;
  struct stat currStat;
  int lastestChangeTime;
  DIR *dr = opendir(".");


  if (dr == NULL) {   // opendir returns NULL if couldn't open directory{
        printf("Could not open current directory\n" );
  }
  if( de = readdir(dr) != NULL) {
    mostRecentDir = de;
    stat(de->d_name, &currStat);
    lastestChangeTime = currStat.st_mtime;
  }
  while ((de = readdir(dr)) != NULL) {
    stat(de->d_name, &currStat);
    if ( (currStat.st_mode & S_IFDIR) && currStat.st_mtime > lastestChangeTime ) {
      mostRecentDir = de;
      lastestChangeTime = currStat.st_mtime;
    }
  }

  //printf("%s was last modified\n", mostRecentDir->d_name);
  closedir(dr);

  //Now at the correct dir, and about to filter through rooms
  dr = opendir(mostRecentDir->d_name);
  if (dr == NULL) {   // opendir returns NULL if couldn't open directory{
        printf("Could not open current directory" );
  }

  mostRecentDirStr = (char*)malloc(sizeof(char) * ( strlen(mostRecentDir->d_name) + 1 ));
  strcpy(mostRecentDirStr, mostRecentDir->d_name);

  //Going through each room file, building structs as we go
  char fileName[80];
  char buffer[100];
  memset(fileName, '\0', sizeof(fileName));
  memset(buffer, '\0', sizeof(buffer));
  i = 0;
  while ((de = readdir(dr)) != NULL) {
    if(!strstr(de->d_name, ".txt")){
      continue;
    }
    memset(fileName, '\0', sizeof(fileName));
    printf("mostRecentDirInHere: %s\n", mostRecentDirStr);
    strcpy(fileName, mostRecentDirStr);
    printf("FileName after strcpy: %s\n", fileName);
    //Get file NAME
    strcat(fileName, "/");
    strcat(fileName, de->d_name);

    //Open file
    printf("Opening file: %s\n", fileName);
    FILE *fp = fopen(fileName, "r");

    if(fp == NULL)
      continue;
    printf("Opened file: %s\n", fileName);
    memset(buffer, '\0', sizeof(buffer));
    j = 0;
    while (fgets(buffer, sizeof(buffer), fp)) {
      buffer[strlen(buffer)-1]='\0';
      if( strstr(buffer, "NAME") ){
        strcpy(rooms[i].roomName, &buffer[11]);
        printf("RoomName: %s\n", rooms[i].roomName);
      }
      else if( strstr(buffer, "CONNECTION") ){
        strcpy(rooms[i].outboundConnections[j], &buffer[14]);
        //printf("RoomConnection %s, numConnections: %d, j:%d\n", rooms[i].outboundConnections[j], rooms[i].numConnections, j);
        rooms[i].numConnections += 1;
        j+=1;
      }
      else if( strstr(buffer, "TYPE") ){
        strcpy(rooms[i].roomType, &buffer[11]);
        //printf("RoomType: %s\n", rooms[i].roomType);
      }
      // printf("Buffer was: %s\n", buffer);
      memset(buffer, '\0', sizeof(buffer));
    }
    fclose(fp);
    i+=1;
  }
  closedir(dr);
  free(mostRecentDirStr);
}

char * getStartRoom(){
  int i;
  for (i = 0; i < 7; i++){
    if (strstr(rooms[i].roomType, "START")){
      //printf("roomType found: %s\n",rooms[i].roomType);
      return rooms[i].roomName;
    }
  }
}

struct Room* getRoomFromName(char *roomName){
  int i;
  for (i = 0; i < 7; i++){
    //printf("RoomName: %s, passedVal: %s\n", rooms[i].roomName, roomName);
    if(strstr(rooms[i].roomName, roomName))
      return &rooms[i];
  }
  return NULL;
}


void printConnections(char *roomName){
  struct Room* currRoom;
  int i;
  if( (currRoom = getRoomFromName(roomName)) == NULL ) {
    printf("Bad room input");
  }
  else{
    printf("POSSIBLE CONNECTIONS:");
    for (i = 0; i < currRoom->numConnections; i++){
      printf("  %s  ", currRoom->outboundConnections[i]);
    }
    printf("\n");
  }
}

//Current location
//Check if current room is END_ROOM
//Print Available locations
// Get user input
//Reprmpt if no valid room was selected
// Add current room to char array
// Move to next room.

// int isEndRoom(char * currRoomName){
//   struct Room* currRoom;
//   currRoom = getRoomFromName(currRoomName);
//   if(strstr(currRoom->roomType, "END"))
//     return 1;
//   return 0;
// }

int isValidMove(struct Room *currRoom, char *nextRoom){
  int i;
  if(strcmp(nextRoom, "time") == 0){
    return 2;
  }
  for ( i = 0; i < currRoom->numConnections; i++){
    if(strcmp(currRoom->outboundConnections[i], nextRoom) == 0)
      return 1;
  }
  printf("\nHUH? I DON’T UNDERSTAND THAT ROOM. TRY AGAIN.\n\n");
  return 0;
}


void addMoveToPath(char *roomToAdd){
  numPaths += 1;
  char** tempPath;
  int i;
  tempPath = malloc(sizeof(char*) * numPaths);
  for (i = 0; i < numPaths; i++){
    tempPath[i] = malloc(sizeof(char) * 3);
  }
  for (i = 0; i < numPaths - 1; i++){
    strcpy(tempPath[i], roomPath[i]);
    free(roomPath[i]);
  }
  strcpy(tempPath[numPaths - 1], roomToAdd);
  free(roomPath);
  roomPath = tempPath;

}

void printPath(){
  int i;
  printf("YOU TOOK %d STEPS. YOUR PATH TO VICTORY WAS:\n", numPaths);
  for (i = 0; i < numPaths; i++)
    printf("%s\n",roomPath[i]);
}


void saveTime(){
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  pthread_mutex_lock(&MUTEX);

  //Save to file
  int fd;
  time_t timeVar;
  struct tm* timeStruct;
  char timeData[256];
  memset(timeData, '\0', sizeof(timeData));

  time(&timeVar);
  remove("currentTime.txt");
  int fp;
  fp = open("currentTime.txt", O_WRONLY | O_CREAT, S_IRWXU);
  timeStruct = localtime(&timeVar);
  strftime(timeData, sizeof(timeData), "%I:%M%P, %A, %B, %d, %G", timeStruct);
  printf("NOT MY TIME: %s\n", timeData);
  write(fp, timeData, strlen(timeData) * sizeof(char));
  close(fp);

  pthread_mutex_unlock(&MUTEX);
}


void printTime(){
  char buffer[80];
  FILE *fp = fopen("currentTime.txt", "r");
  fgets(buffer, sizeof(buffer), fp);
  printf("\n%s\n", buffer);
}


int main() {
  rebuildRoomsArray();
  struct Room* currRoom;
  char nextRoom[60];
  int i;
  int flag;
  pthread_mutex_lock(&MUTEX);
  pthread_create(&thread, NULL, saveTime, NULL);

  currRoom = getRoomFromName(getStartRoom());
  //Current location
  //Check if current room is END_ROOM
  while(!strstr(currRoom->roomType, "END")){
    //Print current location
    printf("\nCURRENT LOCATION: %s\n", currRoom->roomName);
    //Print Available locations
    printConnections(currRoom->roomName);
    // Get user input
    do{
      printf("WHERE TO? > ");
      fgets(nextRoom, sizeof(nextRoom), stdin);
      nextRoom[strlen(nextRoom)-1] = '\0';
    }
    while( (flag = isValidMove(currRoom, nextRoom)) == 0);
    if(flag == 1){
      addMoveToPath(nextRoom);
      // Move to next room.
      currRoom = getRoomFromName(nextRoom);
    }
    else if(flag == 2){
      pthread_mutex_unlock(&MUTEX);
      pthread_join(thread, NULL);
      printTime();
      pthread_mutex_lock(&MUTEX);
      pthread_create(&thread, NULL, saveTime, NULL);
    }
  }
  printf("YOU HAVE FOUND THE END ROOM. CONGRATULATIONS!\n");
  printPath();

  for ( i = 0; i < numPaths; i++)
    free(roomPath[i]);
  free(roomPath);
  return 0;
}
