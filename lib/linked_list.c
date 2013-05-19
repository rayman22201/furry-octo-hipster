/**
* @File linked_list.c
* CS 470 Assignment 2
* Ray Imber
* Implements a simple linked list data structure. Singly Linked for simplicity.
* NOTE: The first node has NULL data by convention.
* The thread safe version of each function is suffixed with ts, i.e. linkedList_addNode_ts() 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef void (*print_node_data_function)(void*);
typedef void* (*iterateFunction)(void*);

typedef struct
{
  void* data;
  void* nextNode;
} linkedListNode;

typedef struct
{
  linkedListNode* head;
  linkedListNode* tail; // I'm cheating, but screw it :-P
  linkedListNode* curNode; // Used to implement an iterator.
  unsigned int numNodes;
  pthread_mutex_t lock;
} linkedListStruct;

linkedListStruct* linkedList_newList()
{
  linkedListStruct* newList;
  linkedListNode* newHead;
  
  newHead = malloc(sizeof(linkedListNode));
  newHead->data = NULL;
  newHead->nextNode = NULL;

  newList = malloc(sizeof(linkedListStruct));
  newList->head = newHead;
  newList->curNode = newHead;
  newList->tail = newHead;

  newList->numNodes = 0;

  return newList;
}

linkedListStruct* linkedList_newList_ts()
{
  linkedListStruct* newList;
  newList = linkedList_newList();
  pthread_mutex_init( &(newList->lock), NULL );
  return newList;
}

int linkedList_isEmptyList(linkedListStruct* cur_list)
{
  return ( cur_list->head == cur_list->tail );
}

int linkedList_isEmptyList_ts(linkedListStruct* cur_list)
{
  pthread_mutex_lock(&(cur_list->lock));
  int retVal = linkedList_isEmptyList(cur_list);
  pthread_mutex_unlock(&(cur_list->lock));
  
  return retVal;
}

linkedListNode* linkedList_addNode(linkedListStruct* cur_list, void* newData)
{
  linkedListNode* tailNode;
  linkedListNode* oldNext;
  linkedListNode* newNode;

  tailNode = cur_list->tail;
  
  oldNext = tailNode->nextNode;

  newNode = malloc(sizeof(linkedListNode));
  newNode->data = newData;
  newNode->nextNode = NULL;

  tailNode->nextNode = newNode;
  cur_list->tail = newNode;
  cur_list->numNodes++;

  return newNode;
}

linkedListNode* linkedList_addNode_ts(linkedListStruct* cur_list, void* newData)
{
  pthread_mutex_lock(&(cur_list->lock));
  linkedListNode* retVal = linkedList_addNode(cur_list, newData);
  pthread_mutex_unlock(&(cur_list->lock));
  return retVal;
}

void linkedList_reset_iterator(linkedListStruct* cur_list)
{
  cur_list->curNode = cur_list->head;
}

void linkedList_reset_iterator_ts(linkedListStruct* cur_list)
{
  pthread_mutex_lock(&(cur_list->lock));
  linkedList_reset_iterator(cur_list);
  pthread_mutex_unlock(&(cur_list->lock));
}

linkedListNode* linkedList_iterate(linkedListStruct* cur_list)
{
  if(cur_list->curNode != NULL)
  {
    cur_list->curNode = (cur_list->curNode)->nextNode;
    return cur_list->curNode;
  }
  else
  {
    cur_list->curNode = cur_list->head;
    return NULL;
  }
}

linkedListNode* linkedList_iterate_ts(linkedListStruct* cur_list)
{
  pthread_mutex_lock(&(cur_list->lock));
  linkedListNode* retVal = linkedList_iterate(cur_list);
  pthread_mutex_unlock(&(cur_list->lock));
  return retVal;
}

void* linkedList_foreach(linkedListStruct* cur_list)
{
  linkedListNode* curNode;
  curNode = linkedList_iterate(cur_list);
  if(curNode != NULL)
  {
    if(curNode->data != NULL)
    {
      return (curNode->data);
    }
    else
    {
      return linkedList_foreach(cur_list);
    }
  }
  return NULL;
}

void* linkedList_foreach_ts(linkedListStruct* cur_list)
{
  pthread_mutex_lock(&(cur_list->lock));
  void* retVal = linkedList_foreach(cur_list);
  pthread_mutex_unlock(&(cur_list->lock));
  return retVal;
}

void linkedList_removeNode(linkedListStruct* cur_list, linkedListNode* nodeToDelete)
{
  linkedListNode* prevNode;
  linkedListNode* curNode;
  prevNode = NULL;
  curNode = NULL;

  linkedList_reset_iterator(cur_list);
  prevNode = cur_list->head;

  do
  {
    curNode = linkedList_iterate(cur_list);
    //NOTE: comparing addresses!
    if(curNode == nodeToDelete)
    {
      linkedListNode* newNext;
      newNext = curNode->nextNode;
      prevNode->nextNode = newNext;
      if(curNode == cur_list->tail)
      {
        cur_list->tail = prevNode;
      }
      free(curNode);
      cur_list->numNodes--;
      break;
    }
    prevNode = curNode;
  } while(curNode != NULL);
}

void linkedList_removeNode_ts(linkedListStruct* cur_list, linkedListNode* nodeToDelete)
{
  pthread_mutex_lock(&(cur_list->lock));
  linkedList_removeNode(cur_list, nodeToDelete);
  pthread_mutex_unlock(&(cur_list->lock)); 
}

void linkedList_printList(linkedListStruct* cur_list, print_node_data_function output)
{
  if(cur_list == NULL)
  {
    printf("Invalid Linked List");
    return;
  }
  
  linkedList_reset_iterator(cur_list);
  linkedListNode* curNode;
  do
  {
    curNode = linkedList_iterate(cur_list);
    if(curNode != NULL)
    {
      output(curNode->data);
    }
  } while(curNode != NULL);
}

void linkedList_findAndRemoveNode(linkedListStruct* cur_list, void* dataToFind)
{
  if(dataToFind != NULL)
  {
    linkedList_reset_iterator(cur_list);
    linkedListNode* prevNode;
    linkedListNode* curNode;
    prevNode = cur_list->head;
    curNode = NULL;
    do
    {
      curNode = linkedList_iterate(cur_list);
      if(curNode != NULL)
      {
        //NOTE: comparing addresses!
        if(curNode->data == dataToFind)
        {
          linkedListNode* newNext;
          newNext = curNode->nextNode;
          prevNode->nextNode = newNext;
          if(curNode == cur_list->tail)
          {
            cur_list->tail = prevNode;
          }
          free(curNode);
          cur_list->numNodes--;
          break;
        }
      }
      prevNode = curNode;
    } while(curNode != NULL);
    linkedList_reset_iterator(cur_list);
  }
}

void linkedList_findAndRemoveNode_ts(linkedListStruct* cur_list, void* dataToFind)
{
  pthread_mutex_lock(&(cur_list->lock));
  linkedList_findAndRemoveNode(cur_list, dataToFind);
  pthread_mutex_unlock(&(cur_list->lock));
}

void linkedList_freeAllButFirst(linkedListStruct* cur_list)
{
  linkedListNode* firstNode;
  firstNode = cur_list->head->nextNode; // First node with real data
  if(firstNode != NULL)
  {
    cur_list->curNode = firstNode->nextNode;
    linkedListNode* curNode;
    do
    {
      curNode = linkedList_iterate(cur_list);
      if(curNode != NULL)
      {
        if(curNode->data != NULL)
        {
          free(curNode->data);
          curNode->data = NULL;
        }
        free(curNode);
      }
    } while(curNode != NULL);
    firstNode->nextNode = NULL;
    cur_list->tail = firstNode;
    cur_list->numNodes = 1;
  }
}

void linkedList_freeAllButFirst_ts(linkedListStruct* cur_list)
{
  pthread_mutex_lock(&(cur_list->lock));
  linkedList_freeAllButFirst(cur_list);
  pthread_mutex_unlock(&(cur_list->lock));
}

void* linkedList_pop(linkedListStruct* cur_list)
{
  if(!linkedList_isEmptyList(cur_list))
  {
    // The true head is a placeholder, so get the real Head, which is the second Node.
    linkedListNode* realHead = cur_list->head->nextNode;
    void* retVal = realHead->data;
    linkedList_removeNode(cur_list, realHead);
    return retVal;
  }
  return NULL;
}

void* linkedList_pop_ts(linkedListStruct* cur_list)
{
  pthread_mutex_lock(&(cur_list->lock));
  void* retVal = linkedList_pop(cur_list);
  pthread_mutex_unlock(&(cur_list->lock));
  return retVal;
}

void linkedList_free(linkedListStruct* cur_list)
{
  cur_list->curNode = cur_list->head; // Reset the iterator to the head
  linkedListNode* curNode;
  do
  {
    curNode = linkedList_iterate(cur_list);
    free(curNode);
  } while(curNode != NULL);

  free(cur_list);
}

void linkedList_free_ts(linkedListStruct* cur_list)
{
  cur_list->curNode = cur_list->head; // Reset the iterator to the head
  linkedListNode* curNode;
  do
  {
    curNode = linkedList_iterate(cur_list);
    free(curNode);
  } while(curNode != NULL);
  
  pthread_mutex_destroy(&(cur_list->lock));

  free(cur_list);
}

/**
* --------------------------------Interface Functions for use with the Hash Table Library---------------------------------------
* @see hash_table.c
* @TODO: implement thread safety
*/

void linkedList_free_function(void* curHash)
{
  linkedListStruct* elementList;
  elementList = (linkedListStruct*)curHash;
  linkedList_free_ts(elementList);
}

void linkedList_init_function(void** curHash)
{
  (*curHash) = linkedList_newList_ts();
}

void linkedList_add_function(void** curHash, void* newElement)
{
  linkedListStruct* elementList;
  elementList = (linkedListStruct*)(*curHash);

  //add the node to the list
  linkedList_addNode_ts(elementList, newElement);
}

void linkedList_remove_function(void** curHash, void* elementToRemove)
{
  if((*curHash) != NULL)
  {
    linkedListStruct* elementList;
    elementList = (linkedListStruct*)(*curHash);
    linkedList_findAndRemoveNode_ts(elementList, elementToRemove);
  }
}
