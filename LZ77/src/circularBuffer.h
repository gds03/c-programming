#ifndef _CIRCULAR_BUFFER_H
#define _CIRCULAR_BUFFER_H


#include <stdlib.h>
#include <crtdbg.h>
#include <stdio.h>
#include "base.h"

#define incrementPtr(buffer,ptr) if( (ptr + 1) >= (buffer->data + buffer->buffer_size) ) ptr = buffer->data; \
								 else ptr = ptr + 1;



typedef struct _ringbufferchar
{
	char *data;
	char *start, *finish, *endTw;
	boolean circular;

	int current_lookahead_size;

	// This fields are for readonly purposes
	int buffer_size;
	int lookahead_size;
} RingBufferChar, *PRingBufferChar;

//
// Use this method to receive a ringbuffer instance
// 
PRingBufferChar 
__stdcall 
newInstance(
	__in int buffer_size,
	__in int lookahead_size
);

//
// Use this method to release one ringbuffer
//
void 
__stdcall
deleteInstance(
	__in PRingBufferChar buffer
);

//
// Use this method to fill the lookahead with characters.
// Typically, this method is called at first phase, that is,
// when the lookahead is empty and needs to be filled to start the
// compressing stage
//
void 
__stdcall
fillLookahead(
	__in PRingBufferChar buffer, 
	__in char theChar
);

//
// Use this method to decrement the lookahead.
// Typically, this method is called once you not have any character
// to compress.
// 
void 
__stdcall
decrementLookahead(
	__in PRingBufferChar buffer, 
	__in int units
);

//
// Use this method to put a char in a circular manner on a ringbuffer.
// Internally, this method increments finish and endTW (slide the lookahead)
// one unit by each call, and if the buffer is already circular, start ptr 
// starts sliding too.
// 
void 
__stdcall
putChar(
	__in PRingBufferChar buffer,
	__in char theChar
);


//
// Use this method to put a char ir a circular manner, but this method
// should only be used by the decompressor, because the decompressing stage,
// does not contain lookahead region, and so, this method don't treat lookahead
// region.
//
void
__stdcall
putTextWindow(
	__in PRingBufferChar buffer,
	__in char theChar
);




#endif