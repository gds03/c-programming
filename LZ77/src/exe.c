#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CircularBuffer.h"


#define PAK_EXTENSION ".pak\0"
#define PAK_LENGTH 5
#define CHAR_TOKEN 0
#define PHRASE_TOKEN 1
#define CHAR_SIZE_BITS sizeof(char) * 8
#define CHAR_TOKEN_BITS 9
#define TOKEN_PHRASE_BIT 0x40


//
// Global Variables (This is a single thread application)
// 

FILE* source;
FILE* destination;
char* originExtension;

int twNecessaryBits;
int lhNecessaryBits;
int phraseTokenBits;

unsigned char bufferToFile;
char bufferIdx;



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

int 
getNecessaryMaskFor(__in int bits)
{
	return (1 << bits) - 1;
}

int 
getNecessaryBitsFor(__in int value) 
{
	int count = 1;
	int internalValue = 1;
	int slider = 1;

	while(internalValue < value) {
		internalValue |= (slider <<= 1);
		count++;
	}

	return count;
}


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

	*occurrences = itMaxOccurrences;
	return itMax;
}


void 
addBuffer(
	__in boolean phrase,
	__in int distance,
	__in int occurrences,
	__in unsigned char character
) 
{
	int numShifts = 1 + bufferIdx;			// char token + other bits already on character
	int missingBits;
	unsigned char higherPart, lowerPart;

	if( phrase ) {
		
		//
		// Write distance to text window and number of occurrences
		//
		
		character &= 0;
		character = 0x80;					// phrase token
		character |= (distance << 3);		// distance with 4 bits
		character |= (--occurrences);		

		numShifts--;
		bufferIdx += 6;
		missingBits = (CHAR_SIZE_BITS + 1) - bufferIdx;
	}
	else {
		bufferIdx += CHAR_SIZE_BITS;
		missingBits = numShifts;
	}

	higherPart = character >> numShifts;					// With Char token Inclusivé
	lowerPart = character & getNecessaryMaskFor(numShifts); // Remaining lower part bits
	bufferToFile |= higherPart;

	if(bufferIdx >= CHAR_SIZE_BITS) {
			
		// WriteToFile();
			
		bufferToFile &= 0;
		bufferToFile = lowerPart << (CHAR_SIZE_BITS - missingBits);
			
		bufferIdx = bufferIdx % (CHAR_SIZE_BITS - 1);
	}	
}


void 
doCompression() 
{
	PRingBufferChar buffer = newInstance();

	boolean achievedEOF = FALSE;

	int i = 0;
	unsigned char c;

	char* itMax;
	int itMaxOccurrences = 0;
	int* itMaxPtr = &itMaxOccurrences;


	twNecessaryBits = getNecessaryBitsFor(buffer_dim - lookahead_dim);
	lhNecessaryBits = getNecessaryBitsFor(lookahead_dim) - 1;			// -1 is necessary because for example 4 occurrences we can represent with 11 (2bits)
	phraseTokenBits = 1 + twNecessaryBits + lhNecessaryBits;			// 1 because phrase token

	

	//
	// Fill lookahead buffer stage
	// 

	while( (c = fgetc(source)) != EOF && i++ < lookahead_dim )
		fillLookahead(buffer, c);

	//
	// Start compressing (until end of file)
	// 

	while( !achievedEOF )
	{
		int distance;
		int operations;
		boolean phrase;

		itMax = searchItMax(buffer, itMaxPtr);
		operations = itMax == NULL ? 1 : *itMaxPtr;
		phrase = itMax != NULL;

		distance = (itMax < buffer->endTw) ? (buffer->endTw - itMax) 
										   : (( buffer->data + buffer_dim - itMax) + (buffer->endTw - buffer->data));	// sums the endTw distance + itMax until end of the tw

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
	}

	

	



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






