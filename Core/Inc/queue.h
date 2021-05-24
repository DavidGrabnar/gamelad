/*
 * queue.h
 *
 *  Created on: 18 May 2021
 *      Author: GrabPC
 */

#ifndef INC_QUEUE_H_
#define INC_QUEUE_H_

struct Queue* createQueue(int capacity);
int isFull(struct Queue* queue);
int isEmpty(struct Queue* queue);
void enqueue(struct Queue* queue, int item);
int dequeue(struct Queue* queue);
int front(struct Queue* queue);
int back(struct Queue* queue);
int* elements(struct Queue* queue);
int size(struct Queue* queue);

#endif /* INC_QUEUE_H_ */
