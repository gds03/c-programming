#include <stdio.h>

#define buffer_dim 16
#define lookahead_dim 4

#define incrementPtr(buffer,ptr) if(ptr + 1 >= buffer->data + buffer_dim) ptr = buffer->data; \
								 else ptr = ptr + 1;

typedef enum _boolean
{
	FALSE = 0,
	TRUE = 1
} boolean;

typedef struct _ringbufferchar
{
	char data[buffer_dim];
	char *start, *finish, *endTw;
	boolean circular;
	int lookahead_size;
} RingBufferChar, *PRingBufferChar;

//
// Use this method to receive a ringbuffer instance
// 
PRingBufferChar newInstance();

//
// Use this method to release one ringbuffer
//
void deleteInstance(__in PRingBufferChar buffer);

//
// Use this method to fill the lookahead with characters.
// Tipically, this method is called at first phase, that is,
// when the lookahead is empty.
//
void
fillLookahead(
	__in PRingBufferChar buffer, 
	__in char theChar
);

//
// Use thid method to decrement the lookahead.
// Tipically, this method is called once you not have any character
// to compress - decrementing the lookahead
// 
boolean 
decrementLookahead(
	__in PRingBufferChar buffer, 
	__in int units
);

//
// Use this method to put a char in a circular manner on a ringbuffer.
// Internally, this method increments finish and endTW (slide the lookahead)
// one unit by each call.
// 
void
putChar(
	__in PRingBufferChar buffer,
	__in char theChar
);




