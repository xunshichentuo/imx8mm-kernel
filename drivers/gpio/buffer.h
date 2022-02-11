#ifndef __MY_BUFF__
#define __MY_BUFF__

#define BufferSize 128

struct QNode {
	int Data[BufferSize];
	int rear;
	int front;
};

typedef struct QNode *Queue;

bool IsEmpty(Queue Q);
void AddQ(Queue PtrQ, int item);
int DeleteQ(Queue PtrQ);

bool IsEmpty(Queue Q)
{
	return (Q->rear == Q->front);
}

void AddQ(Queue PtrQ, int item)
{
	if((PtrQ->rear+1)%BufferSize == PtrQ->front) {
		printk("%s, Queue full\n", __FUNCTION__);
		return;
	}

	PtrQ->rear = (PtrQ->rear+1)%BufferSize;
	PtrQ->Data[PtrQ->rear] = item;
}

int DeleteQ(Queue PtrQ)
{
	if(IsEmpty(PtrQ)) {
		printk("%s,Queue empty\n", __FUNCTION__);
		return -1;
	} else {
		PtrQ->front = (PtrQ->front+1)%BufferSize;
		return PtrQ->Data[PtrQ->front];
	}
}


#endif
