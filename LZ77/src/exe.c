#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "CircularBuffer.h"




#define PAK_EXTENSION ".pak\0"
#define PAK_LENGTH 5

#define BUFFER_DIM 4


//
// Tests
// 

void TestCircularBuffer()
{
	PRingBufferChar buffer = newInstance();
	
	// 
	// Fill lookahead
	fillLookahead(buffer, 'c');
	fillLookahead(buffer, 'o');
	fillLookahead(buffer, 'o');
	fillLookahead(buffer, 'o');

	putChar(buffer, 'k');
	putChar(buffer, 'b');
	putChar(buffer, 'o');
	putChar(buffer, 'o');
	putChar(buffer, 'o');
	putChar(buffer, 'k');
	putChar(buffer, 'o');
	putChar(buffer, 'k');
	putChar(buffer, 'o');
	putChar(buffer, 'k');
	putChar(buffer, 'o');
	putChar(buffer, 'k');

	putChar(buffer, 'x');
	putChar(buffer, 'y');

	decrementLookahead(buffer, 3);

	deleteInstance(buffer);
}






//
// Helper methods
// 


void
copyCharsAddPakExtension(
	__in char* source, 
	__in char* destination, 
	__in int count
) {

	for(; count > 0; count--, destination++, source++) {
		*destination = *source;
	}

	// 
	// Add pak extension
	strcpy(destination, PAK_EXTENSION);
}

boolean
getDestinationFromSourceFilename(
	__in char* sourceFilename,
	__out FILE* destination,
	__out char* originExtension)
{

	//
	// 'example.xslt' -> 'example.pak' | 'example' -> 'example.pak'
	// 

	int destLen, copyTo;
	int sourceLen = strlen(sourceFilename);	
	char* ptrLastPoint = strrchr(sourceFilename, '.');
	char* destFilename;
	
	if(ptrLastPoint == NULL) {

		//
		// File without extension
		// 
		copyTo = sourceLen;
		destLen = sourceLen + PAK_LENGTH;						// Name of the file plus .pak\0
		originExtension = NULL;									// Tells that source file haven't extension!	
	}
	else {

		//
		// File with extension
		// 

		int originExtLen = strlen(ptrLastPoint);		// Count with . in this measure.
		originExtension = (char*) malloc(originExtLen);
		strcpy(originExtension, ++ptrLastPoint);

		//

		copyTo = sourceLen - originExtLen;
		destLen = copyTo + PAK_LENGTH;		
	}

	//
	// Build destination name
	// 
	
	destFilename = (char*) malloc(destLen);
	copyCharsAddPakExtension(sourceFilename, destFilename, copyTo);


	//
	// Try open the file for writing..
	// 
	destination = fopen(destFilename, "wb");

	//
	// We don't need destination filename after this instruction, so we can free the memory!
	// 
	free(destFilename);		


	if(destination == NULL) {
		free(originExtension);
		return FALSE;
	}

	return TRUE;
}

//
// Must verify if arguments are valid and a file exists on disk with the name of the first argument.
// If the file exists, must create a file dest and return a file handle in output parameter
// and return true indicating that the call executed with sucess.
// 
boolean 
tryInitialize(
	__in int argc,
	__in char *argv[],
	__out FILE* source,
	__out FILE* destination,
	__out char* originExtension
) {
	char *sourceFilename;

	//
	// Verify if user specified the arguments (filename)
	// 

	if(argc <= 1) {
		fprintf(stderr, "\n You must specify the filename to compress");
		return FALSE;
	}

	sourceFilename  = argv[1];


	//
	// Try open the file handle to the file
	// 
	source = fopen(sourceFilename, "rb");

	if(source == NULL) {
		fprintf(stderr, "\n Error while opening the file");
		return FALSE;
	}


	if(!getDestinationFromSourceFilename(sourceFilename, destination, originExtension)) {
		fprintf(stderr, "\n Error while creating compressed file");
		return FALSE;
	}

	return TRUE;
}



char* 
searchItMax(
	__in PRingBufferChar buffer,
	__out int* occurrences
) {
	char* itMax = NULL;		// Pointer to the best phrase on dictionary
	int itMaxOccurrences = 0;

	char* match;
	int	  matchOccurrences = 0;

	int twIdx = 0;			// Used to know the distance 
	

	char* twPtr = buffer->start;
	char* lhPtr = buffer->endTw;

	boolean stop = FALSE;

	//
	// Search lookahead on text window
	// 

	while(!stop && twPtr != buffer->endTw) 
	{		
		if(*twPtr == *lhPtr) {

			//
			// First character of lookahead word match
			// 

			match = twPtr;

			do {
				matchOccurrences++;

				if(twPtr == buffer->endTw)
					stop = TRUE;

				incrementPtr(buffer, twPtr);
				incrementPtr(buffer, lhPtr);

			} while(matchOccurrences <= lookahead_dim && *twPtr == *lhPtr);

			//
			// Establish the higher match here..
			// 

			if(matchOccurrences > itMaxOccurrences) {
				itMaxOccurrences = matchOccurrences;
				itMax = match;
			}
		}

		//
		// Pos-for body

		incrementPtr(buffer, twPtr)
		twIdx++;
		lhPtr = buffer->endTw;
	}	

	occurrences = itMaxOccurrences;
	return itMax;
}


void 
addBuffer(
	__in char* buffer,
	__in boolean phrase,
	__in int distance,
	__in int occurrences,
	__in char character
) {
	//
	// Todo


}


void 
doCompression(
	__in FILE* source, 
	__in FILE* destination
) {
	PRingBufferChar buffer = newInstance();

	char memBuffer[BUFFER_DIM];
	int memBufferBits = 0;
	int *memBufferBitsPtr = &memBufferBits;

	int i = 0;
	boolean achievedEOF = FALSE;
	char c;

	char* itMax;
	int itMaxOccurrences = 0;
	int* itMaxPtr = &itMaxOccurrences;
	

	//
	// Fill lookahead buffer stage
	// 

	while( (c = fgetc(source)) != EOF && i++ >= lookahead_dim )
		fillLookahead(buffer, c);

	//
	// Start compressing (until end of file)
	// 

	do
	{
		int distance;
		int operations;

		itMax = searchItMax(buffer, itMaxPtr);
		operations = itMax == NULL ? 1 : *itMaxPtr;
		distance = buffer->endTw - itMax;

		//
		// Put character on buffer and if necessary transfer 
		// to destination file
		// 

		addBuffer(memBuffer, itMax != NULL, distance, *itMaxPtr, buffer->endTw);
		

		//
		// Transfer from source file, operations characters
		// 

		while(operations-- > 0) {
			c = fgetc(source);

			if(c == EOF) {
				achievedEOF = TRUE;
				break;
			}
			
			// 
			// Put char on ringbuffer and advance lookahead
			// 
			putChar(buffer, c);
		}

	}while(!achievedEOF);

	

	



	deleteInstance(buffer);
}

// *******************************************************************************************************************
//															Executable
// *******************************************************************************************************************


int main(int argc, char *argv[])
{
	FILE* source = NULL;
	FILE* destination = NULL;
	char* originExtension = NULL;

	//
	// Validate input data & file
	//

	if( !tryInitialize(argc, argv, source, destination, originExtension) )
		return;

	//
	// Here we got the file destination and file source handle plus 
	// origin extension memory allocated
	// 
	

	doCompression(source, destination);


	//
	// Close kernel handle to the file
	//


	fclose(source);
	fclose(destination);


	free(originExtension);

	return 0;
}





