#include "CircularBuffer.h"

#include <stdlib.h>
#include <crtdbg.h>

PRingBufferChar newInstance()
{
	PRingBufferChar ptr = (PRingBufferChar) malloc(sizeof(RingBufferChar));
	_ASSERTE(ptr != NULL);

	ptr->circular = FALSE;
	ptr->endTw = ptr->finish = ptr->start = ptr->data;
	ptr->lookahead_size = 0;

	return ptr;
}

void deleteInstance(__in PRingBufferChar buffer)
{
	_ASSERTE(buffer != NULL);
	free(buffer);
}


void fillLookahead(__in PRingBufferChar buffer, __in char theChar)
{
	_ASSERTE(buffer->lookahead_size < lookahead_dim);

	// 
	// Set char on finish position
	//
	*buffer->finish = theChar;

	//
	// Only increment finish
	//
	incrementPtr(buffer, buffer->finish)

	//
	// Increment lookahead size
	// 
	buffer->lookahead_size++;
}

boolean decrementLookahead(__in PRingBufferChar buffer, __in int units)
{
	_ASSERTE(buffer->lookahead_size - units >= 0);

	while(units-- > 0 && buffer->endTw != buffer->finish)
	{
		//
		// Increment endTW and fix finish position
		// 
		incrementPtr(buffer, buffer->endTw)
	}

	//
	// If endTw is equal to finish, means that method returns FALSE.
	//
	return (boolean)(buffer->endTw != buffer->finish);
}

void putChar(__in PRingBufferChar buffer, __in char theChar)
{
	// 
	// Set char on finish position
	//
	*buffer->finish = theChar;
	buffer->finish++;

	//
	// We can't use macro because when finish pass the boundaries
	// we need to set circular to true.
	// 
	if(buffer->finish >= buffer->data + buffer_dim) {
		buffer->finish = buffer->data;		
		buffer->circular = TRUE;
	}
	
	//
	// Increment endTw
	// 
	incrementPtr(buffer, buffer->endTw)


	//
	// When finish pass the boundaries, the buffer is circular
	// so we need to advance start pointer (TextWindow is the characters between start and endTw)
	// 
	if(buffer->circular) {
		incrementPtr(buffer, buffer->start)
	}
}