#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CircularBuffer.h"


#define PAK_EXTENSION ".pak\0"
#define PAK_LENGTH 5
#define BUFFER_DIM 4


static FILE* source;
static FILE* destination;
static char* originExtension;

static char destBuffer[BUFFER_DIM];
static int	destBufferBits;



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
	__in char* sourceFilename
)
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
// If the file exists, must create a file dest and return true indicating that the call executed with success.
// 
boolean 
prepareSourceAndDestination(
	__in int argc,
	__in char *argv[]
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


	if( !getDestinationFromSourceFilename(sourceFilename) ) {
		fclose(source);
		fprintf(stderr, "\n Error while creating compressed file");
		return FALSE;
	}

	return TRUE;
}


//
// Searches the best match on textwindow within the buffer for
// lookahead word returning in output parameter the matches
//
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
	__in boolean phrase,
	__in int distance,
	__in int occurrences,
	__in char character
) {
	if(phrase) {
		// 
		// 7 bits to code phrase token



	}
	else 
	{


	}
}


void 
doCompression() {
	PRingBufferChar buffer = newInstance();
	int *memBufferBitsPtr = &destBufferBits;
	boolean achievedEOF = FALSE;

	int i = 0;
	char c;

	char* itMax;
	int itMaxOccurrences = 0;
	int* itMaxPtr = &itMaxOccurrences;
	

	//
	// Fill lookahead buffer stage
	// 

	while( (c = fgetc(source)) != EOF && i++ < lookahead_dim )
		fillLookahead(buffer, c);

	//
	// Start compressing (until end of file)
	// 

	do
	{
		int distance;
		int operations;
		boolean phrase;

		itMax = searchItMax(buffer, itMaxPtr);
		operations = itMax == NULL ? 1 : *itMaxPtr;
		distance = buffer->endTw - itMax;
		phrase = itMax != NULL;

		//
		// Put character on buffer and if necessary transfer 
		// to destination file
		// 

		addBuffer(phrase, distance, *itMaxPtr, *buffer->endTw);
		

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
	char* originExtension = NULL;

	//
	// Validate input data & file
	//

	if( !prepareSourceAndDestination(argc, argv) )
		return;

	//
	// Here we got the file destination, file source handle and 
	// origin extension memory allocated to be setted on file header
	// 
	

	doCompression();


	//
	// Close kernel handle to the file
	//


	fclose(source);
	fclose(destination);


	free(originExtension);

	return 0;
}






