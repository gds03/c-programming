#include "CircularBuffer.h"


PRingBufferChar 
__stdcall newInstance(
	__in int buffer_size, 
	__in int lookahead_size
) {
	PRingBufferChar ptr = (PRingBufferChar) malloc(sizeof(RingBufferChar));
	_ASSERTE( ptr != NULL );

	//
	// Defaults
	//

	if( buffer_size <= 0 ) 
		buffer_size = 16;
	
	if( lookahead_size <= 0 )
		lookahead_size = 4;

	//
	// Sets
	// 

	ptr->data = (char*) malloc(buffer_size);
	ptr->circular = FALSE;
	ptr->endTw = ptr->finish = ptr->start = ptr->data;
	ptr->buffer_size = buffer_size;
	ptr->lookahead_size = lookahead_size;
	ptr->current_lookahead_size = 0;

	return ptr;
}

void 
__stdcall
deleteInstance(
	__in PRingBufferChar buffer
)
{
	_ASSERTE( buffer != NULL );
	
	free(buffer->data);
	free(buffer);
}


void 
__stdcall
fillLookahead(
	__in PRingBufferChar buffer, 
	__in char theChar
) {
	_ASSERTE( buffer->current_lookahead_size  <  buffer->lookahead_size );

	// 
	// Set char on finish position
	//
	*buffer->finish = theChar;

	//
	// Only increment finish
	//
	incrementPtr(buffer, buffer->finish)

	//
	// Increment current lookahead size
	// 
	buffer->current_lookahead_size++;
}

void 
__stdcall
decrementLookahead(
	__in PRingBufferChar buffer, 
	__in int units
) {
	_ASSERTE(buffer->current_lookahead_size - units >= 0);

	while( units-- > 0  &&  buffer->endTw != buffer->finish )
	{
		//
		// Increment endTW and fix finish position
		// 
		incrementPtr(buffer, buffer->endTw)
	}
}

void 
__stdcall
putChar(
	__in PRingBufferChar buffer,
	__in char theChar
) {
	// 
	// Set char on finish position
	//
	*buffer->finish = theChar;
	buffer->finish++;

	//
	// We can't use macro because is based on finish 
	// that we set the buffer in circular state.
	// 
	if( buffer->finish >= (buffer->data + buffer->buffer_size) ) {
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
	if( buffer->circular ) {
		incrementPtr(buffer, buffer->start)
	}
}