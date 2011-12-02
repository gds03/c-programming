#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CircularBuffer.h"
#include "pak.h"


#define PAK_EXTENSION ".pak\0"
#define PAK_LENGTH 3
#define HEADER_OFFSET 5
#define CHAR_SIZE_BITS sizeof(char) * 8



// ********************************************************************
//			global variables (this is a single-thread app)
// ********************************************************************


FILE* source;
FILE* destination;
char* originExtension;
unsigned char originExtensionSize;

int twNecessaryBits;
int lhNecessaryBits;
int phraseTokenBits;
long tokensCount;

unsigned char bufferToFile;			// Character buffer to be written to the file
unsigned char bufferIdx;			// This should never be greater than 16


// ********************************************************************
//					unordered methods declarations
// ********************************************************************

void __stdcall WriteLastByteIfNecessary() ;
void __stdcall writeOnFile();


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

//
// Given a number, return the module
// 
unsigned int __stdcall moduleNumber(int n)
{
	return n < 0 ? (-1 * n) : n;
}

//
// Returns one mask for given bits. E.g. bits = 7 returns 1111111 
// 
unsigned int __stdcall getNecessaryMaskFor(unsigned int bits)
{
	return (1 << bits) - 1;
}

//
// Given a number, returns the necessary bits to express the number
//
unsigned int __stdcall getNecessaryBitsFor2(unsigned int value)
{
	unsigned int count = 0;

	while(value != 0) {
		value >>= 1;
		count++;
	}

	return count;
}

//
// Given two strings, copy from source to destinations 'count' characters
// 
void 
__stdcall
copyCharsAddPakExtension(
	__in char* source, 
	__in char* destination, 
	__in int count
) {

	//
	// Copy chars from source to destination
	for( ; count > 0; count--, destination++, source++) {
		*destination = *source;
	}

	// 
	// Add pak extension to destination
	strcpy(destination, PAK_EXTENSION);
}


//
// Searches the best match on textwindow within the buffer for
// lookahead word.
// Returns NULL if no matches were found or a pointer to the best match.
// occurrences contains the number of matches (0 where nothings found)
//
char* 
__stdcall
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
__stdcall
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
		missingBits = moduleNumber(CHAR_SIZE_BITS - numShifts - (phraseTokenBits + 1));	
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


//
// This method fills the lookahead with the first file characters.
// Should be called only when algorithm is starting..
//
void __stdcall fillLookaheadBuffer(PRingBufferChar buffer) 
{
	int i = 0;
	char c;

	while( i++ < lookahead_dim && (c = fgetc(source)) != EOF)
		fillLookahead(buffer, c);
}


//
// This method do the compression algorithm, that is: based on actual lookahead buffer,
// search on textwindow a phrase matching the max size of lookahead. If nothing is found then
// generate a character token and put the next character from source within lookahead.
// If is, generate a phrase token and put maching characters matched on lookahead.
// 
void __stdcall doCompression() 
{
	PRingBufferChar buffer = newInstance();
	boolean stopCompress = FALSE;
	
	char c = NULL;
	char* itMax;
	int itMaxOccurrences = 0;
	int* itMaxOccurrencesPtr = &itMaxOccurrences;


	twNecessaryBits = getNecessaryBitsFor2(buffer_dim - lookahead_dim);
	lhNecessaryBits = getNecessaryBitsFor2(lookahead_dim) - 1;			// -1 is necessary because for example 4 occurrences we can represent with 11 (2bits)
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
		int matchesFound;
		boolean phrase;
		int distance = 0;		


		itMax = searchItMax(buffer, itMaxOccurrencesPtr);
		matchesFound = itMax == NULL ? 1 : *itMaxOccurrencesPtr;
		phrase = (boolean) itMax != NULL;

		if(phrase) {

			//
			// We only need the distance when token is a phrase
			//

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
		// Transfer from source file, matchesFound characters
		// 

		for( ; matchesFound > 0; matchesFound--) {

			if( c != EOF ) {

				//
				// If we not touch the end of file we load the next char to c
				// 

				c = fgetc(source);
			}

			if( c != EOF ) {
				// 
				// Put char on ringbuffer and advance lookahead
				// 

				putChar(buffer, c);
			} 
			else {
				
				//
				// We don't have any character to get from source, 
				// so we decrement the lookahead matchesFound times
				// 

				decrementLookahead(buffer, matchesFound);

				if(buffer->endTw == buffer->finish)
					stopCompress = TRUE;

				//
				// Stop for loop always (we don't want to look matchesFound
				// because we already decremented the lookahead)
				//

				break;	
			}
		}
	}

	//
	// 3rd Stage: We must consult if is data on the buffer and if it is,
	// we must write to the file the final bits
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

boolean __stdcall tryGetSourceFile(const char* fileName)
{
	source = fopen(fileName, "rb");
	return (boolean) source != NULL;
}


// ********************************************************************
//					File destination related methods
// ********************************************************************

//
// This method verify if it is data on the buffer, and if it is, write that data
// to destination file
// Typically is called once you have finish all compression process and
// is data on the buffer that must be written to the file
//
void __stdcall WriteLastByteIfNecessary() 
{
	if(bufferIdx != 0) {
		writeOnFile();
	}
}

//
// This is the only method that write bufferToFile to destination file.
// Automatically increment the tokenCount variable and cleanup the buffer
// for use the next data.
//
void __stdcall writeOnFile()
{
	fputc(bufferToFile, destination);		// Write to file
	tokensCount++;
	bufferToFile = 0;						// Cleanup the buffer 
}

//
// After you have the destination file handle, before you start putting 
// data within, you must advance the pointer first.
// Typically this method should be called before you start compress anything only once.
// Before this space is the header of a pak file.
//
void __stdcall positionFileDestinationPtrToWrite()
{
	fseek(destination, 0, SEEK_SET);
	fseek(destination, sizeof(HeaderPak) + originExtensionSize, SEEK_SET); 
}

//
// This method set the destination file* global variable to file destination (pak)
// Return: true if file was created, and false if not.
//
boolean __stdcall tryCreateDestinationFile(const char* filename) 
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
		destLen = sourceLen + PAK_LENGTH + 2;						// Name of the file plus .pak\0
		originExtension = NULL;									// Tells that source file haven't extension!	
	}
	else {

		//
		// File with extension
		// 

		originExtensionSize = strlen(ptrLastPoint) - 1;
		originExtension = (char*) malloc(originExtensionSize + 1);		// +1 for the \0 terminator
		strcpy(originExtension, ++ptrLastPoint);						// advance ptr to first char


		//

		copyTo = sourceLen - (originExtensionSize + 1);
		destLen = copyTo + PAK_LENGTH + 2;		
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

//
// Before you have all data compressed you should call this method to write 
// the pak header within file.
// Typically this method should be called after you compressed all the file.
//
void __stdcall doHeader() 
{
	PHeaderPak header = (PHeaderPak) malloc(sizeof(HeaderPak));

	header->id[0] = 'P';
	header->id[1] = 'A';
	header->id[2] = 'K';
	header->id[3] = '\0';

	header->offset = sizeof(HeaderPak) + originExtensionSize - HEADER_OFFSET;
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
	DEST_FILE_ERROR_CREATING = 3,
	RETURN_OK = 0
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

	return RETURN_OK;
}






