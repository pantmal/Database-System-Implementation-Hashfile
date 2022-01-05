#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "hash_file.h"

#define RECORDS_NUM 2000 // you can change it if you want
#define BUCKETS_NUM 140  // you can change it if you want
#define DELETIONS 500
#define FILE_NAME "data.db"

char* name(char* baseName, int num, char* suffix) {
  char* newName = malloc(strlen(baseName) + 20);
  strcpy(newName, baseName);
  char numStr[3];
  sprintf(numStr, "%d", num);
  strcat(newName, numStr);
  strcat(newName, suffix);
  return newName;
}

const char* names[] = {
  "Yannis",
  "Christofos",
  "Sofia",
  "Marianna",
  "Vagelis",
  "Maria",
  "Iosif",
  "Dionisis",
  "Konstantina",
  "Theofilos",
  "Giorgos",
  "Dimitris"
};

const char* surnames[] = {
  "Ioannidis",
  "Svingos",
  "Karvounari",
  "Rezkalla",
  "Nikolopoulos",
  "Berreta",
  "Koronis",
  "Gaitanis",
  "Oikonomou",
  "Mailis",
  "Michas",
  "Halatsis"
};

const char* cities[] = {
  "Athens",
  "San Francisco",
  "Los Angeles",
  "Amsterdam",
  "London",
  "New York",
  "Tokyo",
  "Hong Kong",
  "Munich",
  "Miami"
};

#define CALL_OR_DIE(call)     \
  {                           \
    HT_ErrorCode code = call; \
    if (code != HT_OK) {      \
      printf("Error\n");      \
      exit(code);             \
    }                         \
  }

int main() {
  BF_Init(LRU);
  
  CALL_OR_DIE(HT_Init());

  int indexDesc;
  int bucket_counter = 13;

  //The loop works for MAX_OPEN_FILES == 20, cause this is its value in the hash_file.c. If one wishes to change it, be sure to change the number of the loop here too.
  for(int i = 0; i < 22; i++) {   

    indexDesc = -1;
    char* fileName = name("data", i, ".db"); //Each file is named data0.db, data1.db, etc.
    CALL_OR_DIE(HT_CreateIndex(fileName, bucket_counter));
    CALL_OR_DIE(HT_OpenIndex(fileName, &indexDesc));
    free(fileName);

    //If indexDesc doesn't change, openIndex reached the limit
    //of open files. Do nothing.
    if(indexDesc == -1)
      continue;

    Record record;
    srand(12569874);
    int r;
    printf("Insert Entries\n");
    for (int id = 0; id < RECORDS_NUM; ++id) {
      record.id = id;
      r = rand() % 12;
      memcpy(record.name, names[r], strlen(names[r]) + 1);
      r = rand() % 12;
      memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
      r = rand() % 10;
      memcpy(record.city, cities[r], strlen(cities[r]) + 1);

      CALL_OR_DIE(HT_InsertEntry(indexDesc, record));
    }

    printf("RUN PrintAllEntries\n");
    CALL_OR_DIE(HT_PrintAllEntries(indexDesc, NULL));
    

    int id;

    for(int i = 0; i < DELETIONS; i++) {
      id = i * 5;
      printf("Print Entry with id = %d\n", id); 
      CALL_OR_DIE(HT_PrintAllEntries(indexDesc, &id));
      printf("Delete Entry with id = %d\n" ,id);
      CALL_OR_DIE(HT_DeleteEntry(indexDesc, id));
      printf("Print (after delete) Entry with id = %d\n", id); 
      CALL_OR_DIE(HT_PrintAllEntries(indexDesc, &id));
    }

    // Close the first file to test close.
    if(i == 0) {
      CALL_OR_DIE(HT_CloseFile(i));
    }

    bucket_counter = bucket_counter + 50;
  }
  
  for(int i = 0; i < 20; i++) {
    CALL_OR_DIE(HT_CloseFile(i));
  }
  BF_Close();
}