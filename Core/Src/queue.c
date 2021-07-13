/*
 * queue.c
 *
 *  Created on: 18 May 2021
 *      Author: GrabPC
 */

#include <limits.h>
#include <stdlib.h>

struct Queue {
	int front, back, size, capacity;
	int * array;
};

struct Queue* createQueue(int capacity) {
	struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));

	queue->capacity = capacity;
	queue->front = queue->size = 0;

	queue->back = capacity - 1;
	queue->array = (int *) malloc(queue->capacity * sizeof(int));

	return queue;
}

int isFull(struct Queue* queue)
{
    return (queue->size == queue->capacity);
}

int isEmpty(struct Queue* queue)
{
    return (queue->size == 0);
}

void enqueue(struct Queue* queue, int item)
{
    if (isFull(queue))
        return;

    queue->back = (queue->back + 1) % queue->capacity;
    queue->array[queue->back] = item;
    queue->size = queue->size + 1;
}

int dequeue(struct Queue* queue)
{
    if (isEmpty(queue))
        return INT_MIN;
    int item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}

int front(struct Queue* queue)
{
    if (isEmpty(queue))
        return INT_MIN;
    return queue->array[queue->front];
}

int back(struct Queue* queue)
{
    if (isEmpty(queue))
        return INT_MIN;
    return queue->array[queue->back];
}

int size(struct Queue* queue)
{
    return queue->size;
}

// TODO not ideal implementation since it creates a new array, iterator ?
int* elements(struct Queue* queue)
{
	int* array = (int *) malloc(queue->size * sizeof(int));
	for (int i = 0; i < queue->size; i++) {
		array[i] = queue->array[(queue->front + i) % queue->capacity];
	}
	return array;
}
