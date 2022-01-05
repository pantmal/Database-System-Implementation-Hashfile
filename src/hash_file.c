#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hash_file.h"
#define MAX_OPEN_FILES 20

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    BF_PrintError(code);    \
    return HT_ERROR;        \
  }                         \
}

#define OFFSET 2*sizeof(int)

struct openedIndex {
  int fileDescriptor;
  int buckets;
} typedef openedIndex;

openedIndex* tableOfIndexes[MAX_OPEN_FILES];
int openFiles = 0;

// Extremely sophisticated hash function:
int hash(int ID, int buckets) {
  return ID % buckets;
}

void printRecord(Record* record) {
  printf("%d,\"%s\",\"%s\",\"%s\"\n",
    record->id, record->name, record->surname, record->city);
}

HT_ErrorCode HT_Init() {
  //insert code here

  // Initialize all entries of table of indexes to NULL.
  for(int i = 0; i < MAX_OPEN_FILES; i++) {
    tableOfIndexes[i] = NULL;
  }
  return HT_OK;
}

HT_ErrorCode HT_CreateIndex(const char *filename, int buckets) {
  //insert code here

  // Create and open HT file.
  int fd;
  CALL_BF(BF_CreateFile(filename));
  CALL_BF(BF_OpenFile(filename, &fd));

  // Create first block.
  BF_Block *block;
  char* data;
  BF_Block_Init(&block);
  CALL_BF(BF_AllocateBlock(fd, block));
  data = BF_Block_GetData(block);

  // Write "HT" in the beginning of first block to signify
  // we have an HT file.
  char str1[3];
  strcpy(str1, "HT");
  memcpy(data, str1, 2);

  // Write number of buckets after "HT" string.
  memcpy(data+2, &buckets, 4);
  
  // Save changes to first block.
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  // Calculate blocks needed for index.
  int integersInABlock = BF_BLOCK_SIZE / sizeof(int);
  int blocksNeededForIndex = buckets / integersInABlock + 1;

  // Allocate blocks for index.
  for(int i = 0; i < blocksNeededForIndex; i++) {
    CALL_BF(BF_AllocateBlock(fd, block));
    CALL_BF(BF_UnpinBlock(block));
  }

  // Make changes and close HT file.
  BF_Block_Destroy(&block);
  CALL_BF(BF_CloseFile(fd));
  return HT_OK;
}

HT_ErrorCode HT_OpenIndex(const char *fileName, int *indexDesc){
  //insert code here

  // If we have reached max open files, return error.
  if(openFiles == MAX_OPEN_FILES) {
    fprintf(stderr, "Can't open more files.\n");
    return HT_OK;
  }

  openFiles++;

  // Open file.
  int fd;
  CALL_BF(BF_OpenFile(fileName, &fd));

  // Init block.
  BF_Block *block;
  BF_Block_Init(&block);
  char* data;
  CALL_BF(BF_GetBlock(fd, 0, block));
  data = BF_Block_GetData(block);

  // Check if it's not HT file and close it.
  if(strncmp(data, "HT", 2) != 0) {
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    CALL_BF(BF_CloseFile(fd));
    return HT_ERROR;
  }

  // Get number of buckets:
  int buckets;
  memcpy(&buckets, data+2, sizeof(int));

  // Allocate new openedIndex struct.
  openedIndex* newOpenedIndex = (openedIndex*)malloc(sizeof(openedIndex));
  newOpenedIndex->fileDescriptor = fd;
  newOpenedIndex->buckets = buckets;

  // Find first NULL position in table of indexes and put
  // the new openedIndex struct.
  int index = 0;
  while(index < MAX_OPEN_FILES) {
    if(tableOfIndexes[index] == NULL) {
      tableOfIndexes[index] = newOpenedIndex;
      break;
    }
    index++;
  }

  *indexDesc = index;

  // Close block.
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block);

  return HT_OK;
}

HT_ErrorCode HT_CloseFile(int indexDesc) {
  //insert code here

  //Find the index in the table of indexes and close the desired file. 
  openedIndex* indexToDie = tableOfIndexes[indexDesc];
  CALL_BF(BF_CloseFile(indexToDie->fileDescriptor));
  free(indexToDie);
  tableOfIndexes[indexDesc] = NULL;
  openFiles--;

  return HT_OK;
}

HT_ErrorCode HT_InsertEntry(int indexDesc, Record record) {
  //insert code here

  // Get hashed value:
  openedIndex* indexForInsertion = tableOfIndexes[indexDesc];
  int fd = indexForInsertion->fileDescriptor;
  int buckets = indexForInsertion->buckets;
  int hashValue = hash(record.id, buckets);

  // Find block in index:
  int integersInABlock = BF_BLOCK_SIZE / sizeof(int);
  int blockToGoTo = hashValue / integersInABlock + 1; 
  int positionInBlock = hashValue % integersInABlock;

  // Find bucket:
  int bucket;
  BF_Block *indexBlock, *recordBlock;
  BF_Block_Init(&indexBlock);
  BF_Block_Init(&recordBlock);
  char* indexData;
  CALL_BF(BF_GetBlock(fd, blockToGoTo, indexBlock));
  indexData = BF_Block_GetData(indexBlock);
  indexData += positionInBlock * sizeof(int);
  memcpy(&bucket, indexData, sizeof(int));


  // Bucket doesn't exist:
  if(bucket == 0) {
    // Get number of blocks:
    int newBlockNum;
    CALL_BF(BF_GetBlockCounter(fd, &newBlockNum));

    // Create bucket:
    char* recordData;
    CALL_BF(BF_AllocateBlock(fd, recordBlock));
    recordData = BF_Block_GetData(recordBlock);

    // Add new bucket number to index:
    memcpy(indexData, &newBlockNum, sizeof(int));
    BF_Block_SetDirty(indexBlock);
    // CALL_BF(BF_UnpinBlock(indexBlock));

    // Add new record to new bucket:
    int next = -1;
    int count = 1;
    memcpy(recordData, &next, sizeof(int));
    memcpy(recordData+sizeof(int), &count, sizeof(int));
    memcpy(recordData+OFFSET, &record, sizeof(Record));
    BF_Block_SetDirty(recordBlock);
    CALL_BF(BF_UnpinBlock(recordBlock));
    BF_Block_Destroy(&recordBlock);
  }
  else { // Bucket already exists:
    CALL_BF(BF_GetBlock(fd, bucket, recordBlock));
    char* recordData = BF_Block_GetData(recordBlock);
    int next;
    memcpy(&next, recordData, sizeof(int));

    // Find last block in bucket:
    while(next != -1) {
      CALL_BF(BF_UnpinBlock(recordBlock));
      CALL_BF(BF_GetBlock(fd, next, recordBlock));
      recordData = BF_Block_GetData(recordBlock);
      memcpy(&next, recordData, sizeof(int));
    }

    int recordsInBlock = (BF_BLOCK_SIZE - OFFSET) / sizeof(Record);
    int count;
    memcpy(&count, recordData+sizeof(int), sizeof(int));

    // If block is full:
    if(count == recordsInBlock) {
      // Get number of blocks:
      int newBlockNum;
      CALL_BF(BF_GetBlockCounter(fd, &newBlockNum));

      // Add new block number to previous block:
      memcpy(recordData, &newBlockNum, sizeof(int));

      // Save changes to previous block and get rid of it:
      BF_Block_SetDirty(recordBlock);
      CALL_BF(BF_UnpinBlock(recordBlock));

      // Create block:
      CALL_BF(BF_AllocateBlock(fd, recordBlock));
      recordData = BF_Block_GetData(recordBlock);

      // Add new record to new block:
      next = -1;
      count = 1;
      memcpy(recordData, &next, sizeof(int));
      memcpy(recordData+sizeof(int), &count, sizeof(int));
      memcpy(recordData+OFFSET, &record, sizeof(Record));
    }
    //If block isn't full:
    else {
      memcpy(recordData+(OFFSET)+(count*sizeof(Record)), &record, sizeof(Record));
      count++;
      memcpy(recordData+sizeof(int), &count, sizeof(int));
    }

    BF_Block_SetDirty(recordBlock);
    CALL_BF(BF_UnpinBlock(recordBlock));
    BF_Block_Destroy(&recordBlock);
  }

  CALL_BF(BF_UnpinBlock(indexBlock));
  BF_Block_Destroy(&indexBlock);
  return HT_OK;
}

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id) {
  //insert code here

  openedIndex* indexForInsertion = tableOfIndexes[indexDesc];
  int fd = indexForInsertion->fileDescriptor;
  int buckets = indexForInsertion->buckets;

  // Calculate blocks in index.
  int integersInABlock = BF_BLOCK_SIZE / sizeof(int);
  int blocksInIndex = buckets / integersInABlock + 1;

  BF_Block* recordBlock;
  BF_Block_Init(&recordBlock);

  int totalBlocks;
  CALL_BF(BF_GetBlockCounter(fd, &totalBlocks));

  //If id is NULL, print every record in the hash file
  if(id == NULL) { 
    Record* record;
    for(int currentBlock = 1 + blocksInIndex; currentBlock < totalBlocks; currentBlock++){
      CALL_BF(BF_GetBlock(fd, currentBlock, recordBlock));
      char* blockData;
      blockData = BF_Block_GetData(recordBlock);
      int count;
      memcpy(&count, blockData+sizeof(int), sizeof(int));
      for(int i = 0; i < count; i++) {
        record = (Record*)(blockData+OFFSET+i*sizeof(Record));
        printRecord(record);
      }
      CALL_BF(BF_UnpinBlock(recordBlock));
    }
  }

  else { //Otherwise find the record in the hash table using the same operations as in InsertEntry
    Record* record;
    int hashValue = hash(*id, buckets);
    int blockToGoTo = hashValue / integersInABlock + 1; 
    int positionInBlock = hashValue % integersInABlock;

    // Find bucket:
    int bucket;
    BF_Block *indexBlock;
    BF_Block_Init(&indexBlock);
    char* indexData;
    CALL_BF(BF_GetBlock(fd, blockToGoTo, indexBlock));
    indexData = BF_Block_GetData(indexBlock);
    indexData += positionInBlock * sizeof(int);
    memcpy(&bucket, indexData, sizeof(int));

    if(bucket == 0) {
      CALL_BF(BF_UnpinBlock(recordBlock));
      BF_Block_Destroy(&recordBlock);
      printf("Bucket doesn't exist yet.\n");
      return HT_OK;
    }

    int next = bucket;
    int count;
    int printed = 0;
    
    do { //Searching each block in the bucket for the record
      CALL_BF(BF_GetBlock(fd, next, recordBlock));
      char* recordData;
      recordData = BF_Block_GetData(recordBlock);
      memcpy(&next, recordData, sizeof(int));
      memcpy(&count, recordData+sizeof(int), sizeof(int));
      for(int i = 0; i < count; i++) {
        record = (Record*)(recordData+OFFSET+i*sizeof(Record));
        if(record->id == *id) {
          printRecord(record);
          printed++;
        }
      }
      CALL_BF(BF_UnpinBlock(recordBlock));
    } while(next != -1);

    if(printed == 0) {
      printf("Could not find ID %d sadly...\n", *id);
    }
    CALL_BF(BF_UnpinBlock(indexBlock));
    BF_Block_Destroy(&indexBlock); 
  }

  BF_Block_Destroy(&recordBlock);
  return HT_OK;
}

HT_ErrorCode HT_DeleteEntry(int indexDesc, int id) {
  //insert code here

  // Get hashed value:
  openedIndex* indexForInsertion = tableOfIndexes[indexDesc];
  int fd = indexForInsertion->fileDescriptor;
  int buckets = indexForInsertion->buckets;
  int hashValue = hash(id, buckets);

  // Find block in index:
  int integersInABlock = BF_BLOCK_SIZE / sizeof(int);
  int blockToGoTo = hashValue / integersInABlock + 1; 
  int positionInBlock = hashValue % integersInABlock;

  // Find bucket:
  int bucket;
  BF_Block *indexBlock, *recordBlock, *recordBlockLast;
  BF_Block_Init(&indexBlock);
  BF_Block_Init(&recordBlock);
  BF_Block_Init(&recordBlockLast);
  char* indexData;
  CALL_BF(BF_GetBlock(fd, blockToGoTo, indexBlock));
  indexData = BF_Block_GetData(indexBlock);
  indexData += positionInBlock * sizeof(int);
  memcpy(&bucket, indexData, sizeof(int));

  if(bucket == 0) {
      CALL_BF(BF_UnpinBlock(recordBlock));
      BF_Block_Destroy(&recordBlock);
      printf("Bucket doesn't exist yet.\n");
      return HT_OK;
  }

  int next = bucket;
  int count;
  int found = 0;

  Record* recordToDelete = NULL;
  int indexToDelete;
  Record* lastRecord;
  char* recordData, *recordDataLast;

  // Find last record:
  CALL_BF(BF_GetBlock(fd, bucket, recordBlockLast));
  recordDataLast = BF_Block_GetData(recordBlockLast);
  memcpy(&next, recordDataLast, sizeof(int));

  while(next != -1) {
    CALL_BF(BF_UnpinBlock(recordBlockLast));
    CALL_BF(BF_GetBlock(fd, next, recordBlockLast));
    recordDataLast = BF_Block_GetData(recordBlockLast);
    memcpy(&next, recordDataLast, sizeof(int));
  }

  next = bucket;
  
  // Find item to delete.
  do {
    Record* record;
    CALL_BF(BF_GetBlock(fd, next, recordBlock));
    recordData = BF_Block_GetData(recordBlock);
    memcpy(&next, recordData, sizeof(int));
    memcpy(&count, recordData+sizeof(int), sizeof(int));
    for(int i = 0; i < count; i++) {
      record = (Record*)(recordData+OFFSET+i*sizeof(Record));
      if(record->id == id) {
          recordToDelete = record;
          indexToDelete = i;
          break;
      }
    }
    if(recordToDelete != NULL)
      break;
    CALL_BF(BF_UnpinBlock(recordBlock));
  } while(next != -1);

  if(recordToDelete == NULL) {
    printf("Record with ID %d doesn't exist.\n", id);
    CALL_BF(BF_UnpinBlock(recordBlock));
    BF_Block_Destroy(&recordBlock);
    BF_Block_Destroy(&recordBlockLast);
    CALL_BF(BF_UnpinBlock(indexBlock));
    BF_Block_Destroy(&indexBlock);
    return HT_OK;
  }

  

  // Write last record on top of record to delete and 
  // decrease last block count.
  memcpy(&count, recordDataLast+sizeof(int), sizeof(int));
  memcpy(recordData+OFFSET+indexToDelete*sizeof(Record),
    recordDataLast+OFFSET+(count-1)*sizeof(Record), sizeof(Record));
  count--;
  memcpy(recordDataLast+sizeof(int), &count, sizeof(int));

  BF_Block_SetDirty(recordBlock);
  CALL_BF(BF_UnpinBlock(recordBlock));
  BF_Block_Destroy(&recordBlock);

  BF_Block_SetDirty(recordBlockLast);
  CALL_BF(BF_UnpinBlock(recordBlockLast));
  BF_Block_Destroy(&recordBlockLast);

  CALL_BF(BF_UnpinBlock(indexBlock));
  BF_Block_Destroy(&indexBlock);
  return HT_OK;
}