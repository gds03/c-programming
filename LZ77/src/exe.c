#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CircularBuffer.h"
#include "pak.h"


#define PAK_LENGTH 3
#define HEADER_OFFSET 5
#define CHAR_TOKEN_SIZE_BITS 9
#define PHRASE_TOKEN_SIZE_BITS 7
#define CHAR_SIZE_BITS 8



// ********************************************************************
//			global variables (this is a single-thread app)
// ********************************************************************


FILE* source;
FILE* destination;
char* originExtension;
unsigned char originExtensionSize;

unsigned int twNecessaryBits;
unsigned int lhNecessaryBits;
unsigned int phraseTokenBits;
unsigned long tokensCount;

unsigned char bufferToFile;			// Character buffer to be written on the file
unsigned char freePtr;				// Points to the next free bit within bufferToFile character


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
// Returns one mask with all bits with 1 for given bits.
// E.g. bits = 7 returns 1111111 
// 
unsigned int __stdcall getNecessaryMaskFor(unsigned int bits)
{
	return (1 << bits) - 1;
}

//
// Given a number, returns the necessary bits to express the number
//
unsigned int __stdcall getNecessaryBitsFor(unsigned int value)
{
	unsigned int count = 0;

	while(value != 0) {
		value >>= 1;
		count++;
	}

	return count;
}

//
// Given two strings, copy from source to destination, 'count' characters
// and at the end, copy .pak extension
// 
void 
__stdcall
copyCharsAddPakExtension(
	__in char* source, 
	__in char* destination, 
	__in unsigned int count
) {

	//
	// Copy chars from source to destination
	for( ; count > 0; count--, destination++, source++) {
		*destination = *source;
	}

	// 
	// Add pak extension to destination
	strcpy(destination, ".pak\0");
}


//
// Searches the best match on textwindow within the buffer for
// lookahead word.
// Returns NULL if no matches were found or a pointer to the best match.
// occurrences contains the number of matches (0 where nothing's found)
//
char* 
__stdcall
searchItMax(
	__in PRingBufferChar buffer,
	__out unsigned int* occurrences
) {
	char* itMax = NULL;		// Pointer to the best phrase on dictionary
	unsigned int itMaxOccurrences = 0;

	char* match;
	unsigned int matchOccurrences = 0;	

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
			// If we are here, we have the first match of the first character on textwindow

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



//
// Use this method when you need to build a character with phrase token that includes the 
// distance and occurrences. THe format is: 1XXXXYY0, where XXXX is the distance bits and YY is the occurrences
// 
unsigned char
__stdcall 
formatPhraseCharacter(
	__in unsigned int distance,
	__in unsigned int occurrences
) {
	unsigned char c = 0;		// Empty char

	c |= 0x80;
	c |= distance << 3;			
	c |= occurrences << 1;

	return c;
}

void 
__stdcall
addBuffer(
	__in boolean phrase,
	__in unsigned int distance,
	__in unsigned int occurrences,
	__in unsigned char character
) 
{
	unsigned char data = 0;
	unsigned char pendentPart = 0;
	unsigned char characterShifts = 0;
	tokensCount++;

	if( !phrase ) {

		//
		// Character Token.
		// Ever than a char token is created, pendentPart exists and we must preserv old data on the buffer
		// add current data, write data to the file, and add pendentPart to the buffer.

		unsigned char freeBits;
		characterShifts = freePtr + 1;				// +1 for highest bit
		freePtr += CHAR_TOKEN_SIZE_BITS;

		data = character >> characterShifts;		
		pendentPart = character & getNecessaryMaskFor(characterShifts);

		bufferToFile |= data;			// If already exists data on the buffer, we just add.
		writeOnFile();					// Write to the file and clear the buffer

		bufferToFile = pendentPart << (freeBits = CHAR_SIZE_BITS - characterShifts);	// Set pendentPart

		//
		// This is for the case when freePtr is pointing to last free bit (bit0) and that bit is filled with character token 0
		// and we will fill all the buffer with the character ascii code (filling the buffer again) that must be written 
		// to the file.
		// This only happens when freePtr is pointing to the last free bit.
		// 

		if(freeBits == 0)
			writeOnFile();
	} 

	else {

		//
		// Phrase token.
		// When a phrase token is created, there are 3 possibilities.
		// 1 - There is available space for the bits and remains 1 bit
		// 2 - There is available space for the bits and don't remains any bit (must be written to the file)
		// 3 - There is no space and must be splitted in 2 sequences (1 write and 1 set pendentPart)

		characterShifts = freePtr;
		freePtr += PHRASE_TOKEN_SIZE_BITS;
		character = formatPhraseCharacter(distance, --occurrences);

		if(characterShifts == 0) {
				
			//
			// No data available on the buffer.
			// pendentPart doesn't need to be filled and we don't need to write the to the file because 1 bit is missing

			bufferToFile = character;
		}

		else {

			//
			// Some data is available on the buffer and we must preserv it.
			// Must always write to the file, but can or not have pendentPart.
			// 

			data = character >> characterShifts;			
			bufferToFile |= data;					// Preserv old data and add new data.
			writeOnFile();	

			if( characterShifts > 1 ) {

				//
				// pendent part is a subpart of a character bits that doens't were written.
				pendentPart = ((getNecessaryMaskFor(characterShifts - 1) << 1) & character) >> 1;			

				//
				// Restore that bits to the buffer
				bufferToFile = pendentPart << (CHAR_SIZE_BITS - (characterShifts - 1));
			}			
		}
	}

	freePtr = freePtr % CHAR_SIZE_BITS;
}


//
// This method fills the lookahead with the first file characters.
// Should be called only when algorithm is starting..
//
void __stdcall fillLookaheadBuffer(PRingBufferChar buffer) 
{
	unsigned int i = 0;
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
	
	int c = NULL;
	char* itMax;
	unsigned int itMaxOccurrences = 0;
	unsigned int* itMaxOccurrencesPtr = &itMaxOccurrences;


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
		unsigned int distance = 0;	
		unsigned int matchesFound;
		boolean phrase;

		itMax = searchItMax(buffer, itMaxOccurrencesPtr);
		matchesFound = itMax == NULL ? 1 : *itMaxOccurrencesPtr;
		phrase = (boolean) itMax != NULL;

		if( phrase ) {

			//
			// We only need the distance when token is a phrase
			//

			if(itMax < buffer->endTw) {
				distance = buffer->endTw - itMax;
			} 
			else {
				distance = ((buffer->data + buffer_dim) - itMax) + (buffer->endTw - buffer->data);
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
	if(freePtr != 0) {
		writeOnFile();
	}
}

//
// This is the only method that write bufferToFile to destination file.
// Automatically cleanup the buffer for use the next data.
//
void __stdcall writeOnFile()
{
	fputc(bufferToFile, destination);		// Write to file
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