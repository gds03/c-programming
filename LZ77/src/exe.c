#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CircularBuffer.h"
#include "pak.h"


#define PAK_EXTENSION ".pak\0"
#define PAK_LENGTH 5

#define CHAR_SIZE_BITS sizeof(char) * 8

//
// Global Variables (This is a single thread application)
// 


unsigned char necessaryOffset = 3 * sizeof(char) + sizeof(long) + 1;

FILE* source;
FILE* destination;
char* originExtension;
unsigned char originExtensionSize;

int twNecessaryBits;
int lhNecessaryBits;
int phraseTokenBits;
long tokensCount;

unsigned char bufferToFile;
char bufferIdx;


// ********************************************************************
//					unordered methods declarations
// ********************************************************************

void WriteLastByteIfNecessary() ;
void writeOnFile();


// ********************************************************************
//					Tests related methods
// ********************************************************************

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



// ********************************************************************
//					Compression related methods
// ********************************************************************

int positiveNumber(int n)
{
	return n < 0 ? (-1 * n) : n;
}

int getNecessaryMaskFor(int bits)
{
	return (1 << bits) - 1;
}

int getNecessaryBitsFor(int value) 
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

	char* twPtr = buffer->start;
	boolean stop = FALSE;

	//
	// Search lookahead on text window
	// 

	while(!stop && twPtr != buffer->endTw) 
	{	

		//
		// Looking for first character first..
		//

		if(*twPtr == *buffer->endTw) {

			//
			// First character of lookahead word match
			// 

			char* twPtrIter = twPtr;
			char* lhPtrIter = buffer->endTw;

			match = twPtr;

			do {
				matchOccurrences++;

				if(twPtrIter == buffer->endTw)
					stop = TRUE;

				incrementPtr(buffer, twPtrIter)
				incrementPtr(buffer, lhPtrIter)
			}
			while(matchOccurrences <= lookahead_dim && *twPtrIter == *lhPtrIter);

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
		matchOccurrences = 0;
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
	int numShifts = 1 + bufferIdx;			// if bufferIdx != 0 shift always and shift one more for char/phrase token
	int missingBits;
	unsigned char higherPart, lowerPart;


	if( phrase ) {
		
		//
		// Write distance to text window and number of occurrences
		//
		
		character &= 0;										// Reset the character (don't matter because we will code as phrase value)
		character = 0x80;									// phrase token
		character |= (distance << 3);						// distance with 4 bits
		character |= (--occurrences);						// e.g: 1 occurrence is representated as 00

		numShifts--;										// Decrement because phrase token now counts! (char token doesn't)
		bufferIdx += phraseTokenBits;						// We must advance phraseTokenBits on the bufferIdx
		missingBits = positiveNumber(CHAR_SIZE_BITS - numShifts - (phraseTokenBits + 1));	
	}
	else {
		bufferIdx += CHAR_SIZE_BITS;						// We advance bufferIdx 8 units
		missingBits = numShifts;							// Missing bits is always the bits that were shifted
	}

	higherPart = character >> numShifts;					// With Char token Inclusivé
	lowerPart = character & getNecessaryMaskFor(numShifts); // Remaining lower part bits (we get them by making the mask at 1's with the same number of numShifts)
	bufferToFile |= higherPart;								// Fill the buffer whithout smaching the old bits

	//
	// If bufferIdx is higher than a size of a char means that, the character can be written
	// to the file and we need to fill the buffer with the lower bits part
	//

	if(bufferIdx >= CHAR_SIZE_BITS) {						
		writeOnFile();		

		bufferToFile = lowerPart << (CHAR_SIZE_BITS - missingBits);			
		bufferIdx = bufferIdx % (CHAR_SIZE_BITS - 1);
	}	
}



void fillLookaheadBuffer(PRingBufferChar buffer) 
{
	int i = 0;
	char c;

	while( i++ < lookahead_dim && (c = fgetc(source)) != EOF)
		fillLookahead(buffer, c);
}

void doCompression() 
{
	//
	//  Only this method needs the ringbuffer to make the compression
	// 

	// Locals

	PRingBufferChar buffer = newInstance();

	boolean achievedEOF = FALSE;
	boolean stopCompress = FALSE;
	
	char c;
	char* itMax;
	int itMaxOccurrences = 0;
	int* itMaxOccurrencesPtr = &itMaxOccurrences;


	twNecessaryBits = getNecessaryBitsFor(buffer_dim - lookahead_dim);
	lhNecessaryBits = getNecessaryBitsFor(lookahead_dim) - 1;			// -1 is necessary because for example 4 occurrences we can represent with 11 (2bits)
	phraseTokenBits = twNecessaryBits + lhNecessaryBits;			

	
	//
	// 1st Stage: Fill lookahead
	// 

	fillLookaheadBuffer(buffer);	

	//
	// 2nd Stage: Process & Compress
	// 

	while( !stopCompress )
	{
		int distance = 0;
		int operations;
		boolean phrase;

		itMax = searchItMax(buffer, itMaxOccurrencesPtr);
		operations = itMax == NULL ? 1 : *itMaxOccurrencesPtr;
		phrase = (boolean) itMax != NULL;

		//
		// We only need the distance when token is a phrase
		//

		if(phrase) {

			if(itMax < buffer->endTw) {
				distance = buffer->endTw - itMax;
			} 
			else {
				distance = (buffer->data + buffer_dim - itMax) + (buffer->endTw - buffer->data);		// Not tested yet!
			}

		}

		//
		// Put character on buffer and if necessary transfer 
		// to destination file
		// 

		addBuffer(phrase, distance, *itMaxOccurrencesPtr, *buffer->endTw);
		

		//
		// Transfer from source file, operations characters
		// 

		for( ; operations > 0; operations--) {

			if( !achievedEOF ) {

				//
				// If we not touch the end of file we load the next char to c
				// 

				c = fgetc(source);

				if(c == EOF) {
					achievedEOF = TRUE;
				}
			}

			if( !achievedEOF ) {
				// 
				// Put char on ringbuffer and advance lookahead
				// 

				putChar(buffer, c);
			} 
			else {
				
				//
				// We don't have any character to get from source, so we decrement the lookahead operations times
				// 
				decrementLookahead(buffer, operations);

				if(buffer->endTw == buffer->finish)
					stopCompress = TRUE;

				break;	// Stop for loop
			}
		}
	}

	//
	// 3rd Stage: We must consult if is data on the buffer and if it is,
	// we must write to the file the other bits
	// 

	WriteLastByteIfNecessary();
	
	//
	// Free ringbuffer allocated memory!
	// 

	deleteInstance(buffer);
}




// ********************************************************************
//					File source related methods
// ********************************************************************

//
// This method set the source file* global variable to file source to compress
// Return: true if file exists, and false if not.
// 
boolean tryGetSourceFile(const char* fileName)
{
	source = fopen(fileName, "rb");
	return (boolean) source != NULL;
}


// ********************************************************************
//					File destination related methods
// ********************************************************************


void WriteLastByteIfNecessary() 
{
	if(bufferIdx != 0) {
		writeOnFile();
	}
}

void writeOnFile()
{
	fputc(bufferToFile, destination);		// Write to file
	tokensCount++;
	bufferToFile &= 0;
}

void positionFileDestinationPtrToWrite()
{
	fseek(destination, 0, SEEK_SET);
	fseek(destination, necessaryOffset + PAK_LENGTH, SEEK_SET); // 5 is the pak\0 and the offset bytes
}

//
// This method set the destination file* global variable to file destination (pak)
// Return: true if file was created, and false if not.
//
boolean tryCreateDestinationFile(const char* filename) 
{
	//
	// 'example.xslt' -> 'example.pak' | 'example' -> 'example.pak'
	// 

	int destLen, copyTo;
	int sourceLen = strlen(filename);	
	char* ptrLastPoint = strrchr(filename, '.');
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

		int originExtLen = originExtensionSize = strlen(ptrLastPoint);		// Count with . in this measure.
		originExtension = (char*) malloc(originExtLen);
		strcpy(originExtension, ++ptrLastPoint);

		//

		copyTo = sourceLen - originExtLen;
		destLen = copyTo + PAK_LENGTH;		
	}

	//
	// Build destination name
	// 
	
	destFilename = (char*) malloc(destLen);								// Heap alloc
	copyCharsAddPakExtension(filename, destFilename, copyTo);


	//
	// Try open the file for writing..
	// 
	destination = fopen(destFilename, "wb");

	//
	// We don't need destination filename after this instruction, so we can free the memory!
	// 
	free(destFilename);													// Heap free

	return (boolean) destination != NULL;
}

void doHeader() 
{
	PHeaderPak header = (PHeaderPak) malloc(sizeof(HeaderPak));

	header->id[0] = 'P';
	header->id[1] = 'A';
	header->id[2] = 'K';
	header->id[3] = '\0';

	header->offset = necessaryOffset;
	header->position_bits = twNecessaryBits;
	header->coincidences_bits = lhNecessaryBits;
	header->min_coincidences = 1;
	header->tokens = tokensCount;

	fseek(destination, 0, SEEK_SET);
	fwrite(header, sizeof(HeaderPak), 1, destination);
	fwrite(originExtension, originExtensionSize, 1, destination);

	free(header);
}

// *******************************************************************************************************************
//															Executable
// *******************************************************************************************************************


typedef enum applicationResults 
{
	ARGUMENTS_INVALID = 1,
	SOURCE_FILE_NOT_FOUND = 2,
	DEST_FILE_ERROR_CREATING = 3
};

int main(int argc, char *argv[])
{

	//
	// At the first stage we must verify if arguments passed to
	// the application, have filename to compress.
	//

	if(argc < 2)
		return ARGUMENTS_INVALID;

	if( !tryGetSourceFile(argv[1]) ) {
		fprintf(stderr, "File to compress not found \n");
		return SOURCE_FILE_NOT_FOUND;
	}

	if( !tryCreateDestinationFile(argv[1]) ) {
		fprintf(stderr, "Error while creating pak file \n");
		return DEST_FILE_ERROR_CREATING;
	}

	//
	// Here we got the file destination, file source handle that must be closed at the end.
	// The origin extension memory allocated to be setted on file header must be freed at the end.
	// 
	
	positionFileDestinationPtrToWrite();

	doCompression();

	doHeader();

	//
	// Close kernel handle to the file
	//


	fclose(source);
	fclose(destination);


	free(originExtension);

	return 0;
}






