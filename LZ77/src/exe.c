#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "CircularBuffer.h"
#include "pak.h"


#define PAK_LENGTH 3
#define HEADER_OFFSET 5
#define CHAR_TOKEN_SIZE_BITS 9
#define CHAR_SIZE_BITS (sizeof(char) * 8)
#define INT_SIZE_BITS (sizeof(int) * 8)



// ********************************************************************
//			global variables (this is a single-thread app)
// ********************************************************************


FILE* source;
FILE* destination;

char* sourceFileExtension;
unsigned char sourceFileExtensionSize;

unsigned int twNecessaryBits;
unsigned int lhNecessaryBits;
unsigned int phraseTokenBits;
unsigned long tokensCount;

unsigned char fileChar;			// Character buffer to be written on the file
unsigned char freePtr;


// ********************************************************************
//					unordered methods declarations
// ********************************************************************

void __stdcall WriteLastByteIfNecessary() ;
void __stdcall writeToFile();


// ********************************************************************
//					Tests related methods
// ********************************************************************

void TestCircularBuffer()
{
	PRingBufferChar buffer = newInstance(NULL, NULL);
	
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
// Returns NULL if no matches were found, or returns a pointer to the best match.
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
	boolean twCovered = FALSE;

	//
	// Search lookahead on text window
	// 

	while(!twCovered && twPtr != buffer->endTw) 
	{	

		//
		// Looking for first character first..
		//

		if(*twPtr == *buffer->endTw) {

			//
			// If we are here, we have the first match of the first character on textwindow

			char* twPtrIter = twPtr;
			char* lhPtrIter = buffer->endTw;

			match = twPtr;	// Set a reference to the first match

			do {
				matchOccurrences++;

				if(twPtrIter == buffer->endTw) {
					
					// 
					// Here we don't do a break because the phrase can be included on the lookahead
					//
					twCovered = TRUE;	
				}

				incrementPtr(buffer, twPtrIter)
				incrementPtr(buffer, lhPtrIter)
			}
			while( (matchOccurrences < buffer->lookahead_size) && *twPtrIter == *lhPtrIter );

			//
			// Establish the higher match here..
			// 

			if( matchOccurrences > itMaxOccurrences ) {
				itMaxOccurrences = matchOccurrences;
				itMax = match;

				if( matchOccurrences == buffer->lookahead_size ) {

					//
					// This verification is for the case that if we found a phrase on tw with 
					// size of the lookahead, we don't need to continue searching through tw, we can stop!
					//

					break;
				}
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
// Typically called by addBuffer.
// This method returns a integer where the lower bits are the occurrences, the next are the distance and
// the next are the bit at 1 (phrase bit token). For example:
// 1XXXXXXXXYYYY - 8 bits for distance (256 dictionary entries and 16 entries for lookahead
//
unsigned int
__stdcall 
formatPhrase(
	__in unsigned int distance,
	__in unsigned int occurrences
) {
	unsigned char remainingBits = phraseTokenBits;
	unsigned int phraseCh = 0;

	phraseCh = 1 << (--remainingBits);
	phraseCh |= distance << (remainingBits -= twNecessaryBits);
	phraseCh |= occurrences;

	return phraseCh;
}


//
// (One of the most important algorithms of the application)
// Tipically called by doCompression.
// This algorithm have 2 possibles codifications.
// 1. Character token (static 9 bits)
// 2. Phrase token (dynamic x bits)
//
// For Character token:
//		- Union fileChar buffer with higher bits of ch.
//      - call WriteToFile
//		- Set fileChar buffer with lower bits of ch.
//		- Verify if freePtr is pointing to 7 (means that we must call WriteToCall) because
//		  the buffer will be filled and must be written to destination.
//
// For Phrase token:
//		- Get phrase token based on distance and occurrences.
//		- Verify if it is available space on the buffer that we can union the phrase token 
//		  without writing to the file.
//			If is: We just make a union.
//			If not:	We union the higher bits of the phrase and start trying to fill the buffer with
//					8 char bits. When there is impossible, we just set the restant bits to the higher part
//					of the buffer

void 
__stdcall
addBuffer(
	__in boolean phrase,
	__in unsigned int distance,
	__in unsigned int occurrences,
	__in unsigned char ch
) 
{
	if( !phrase ) {

		unsigned char shifts = freePtr + 1;

		fileChar |= ch >> shifts;
		writeToFile();

		fileChar = ( ch & getNecessaryMaskFor(shifts) ) << (CHAR_SIZE_BITS - shifts);

		if( freePtr == (CHAR_SIZE_BITS - 1) ) {
			writeToFile();
		}

		freePtr = (freePtr + CHAR_TOKEN_SIZE_BITS) % CHAR_SIZE_BITS;
	}

	else {

		unsigned int phraseCh = formatPhrase(distance, occurrences);
		unsigned char remainingBits = phraseTokenBits;
		unsigned char freeBits = (CHAR_SIZE_BITS - freePtr);
		char tmp;

		if( (tmp = phraseTokenBits - freeBits) < 0 ) {
			fileChar |= phraseCh << (CHAR_SIZE_BITS - phraseTokenBits);
		}

		else {
			fileChar |= phraseCh >> (phraseTokenBits - freeBits);

			remainingBits -= freeBits;	
			writeToFile();

			while( (remainingBits / CHAR_SIZE_BITS) >= 1 ) {
				unsigned char shifts = (remainingBits - CHAR_SIZE_BITS);

				fileChar = ( phraseCh & (getNecessaryMaskFor(CHAR_SIZE_BITS) << shifts) ) >> shifts;
				writeToFile();
				remainingBits -= CHAR_SIZE_BITS;
			}

			if(remainingBits > 0) {
				
				fileChar = ( phraseCh & getNecessaryMaskFor(remainingBits) ) << (CHAR_SIZE_BITS - remainingBits);
			}
		}

		freePtr = (freePtr + phraseTokenBits) % CHAR_SIZE_BITS;
	}
}


//
// This method fills the lookahead with the first file characters.
// Should be called only when algorithm is starting..
//
void 
__stdcall 
fillLookaheadBuffer(
	__in PRingBufferChar buffer
) {
	unsigned int i = 0;
	char c;

	while( i++ < buffer->lookahead_size && (c = fgetc(source)) != EOF)
		fillLookahead(buffer, c);
}


//
// This method do the compression algorithm, that is: based on actual lookahead buffer,
// search on textwindow a phrase matching the max size of lookahead. If nothing is found then
// generate a character token and put the next character from source within lookahead.
// If is, generate a phrase token and get maching characters from destination on lookahead.
// 
void
__stdcall 
doCompression(const char* buffer_size, const char* lookahead_size) {

	PRingBufferChar buffer = newInstance(atoi(buffer_size), atoi(lookahead_size));
	boolean stopCompress = FALSE;
	
	int c = NULL;
	char* itMax;
	unsigned int itMaxOccurrences = 0;

	twNecessaryBits = getNecessaryBitsFor(buffer->buffer_size - buffer->lookahead_size);
	lhNecessaryBits = getNecessaryBitsFor(buffer->lookahead_size) - 1;	// -1 is necessary because 00 represent 1 occurrence
	phraseTokenBits = twNecessaryBits + lhNecessaryBits + 1;			// +1 for the phrase highest bit

	
	//
	// 1st Stage: Fill lookahead
	// 

	fillLookaheadBuffer(buffer);	

	//
	// 2nd Stage: Process & Compress
	// 

	while( !stopCompress )
	{
		unsigned int	 distance	  = 0;	
		unsigned int	 matchesFound = 1;
		
		boolean codeAsPhrase = FALSE;

		//
		// Search in dictionary
		// 

		itMax = searchItMax(buffer, &itMaxOccurrences);

		if( itMax != NULL ) {		
			
			//
			// As the phrase token is dynamic, we must decide which codification is lower.
			// Code the sequence as a phrase or separate character tokens.
			//
			
			if( phraseTokenBits < (CHAR_TOKEN_SIZE_BITS * itMaxOccurrences) ) {
				codeAsPhrase = TRUE;
				matchesFound = itMaxOccurrences;

				//
				// We only need the distance when token is a phrase
				//

				if(itMax < buffer->endTw) {
					distance = buffer->endTw - itMax;
				} 
				else {
					distance = ((buffer->data + buffer->buffer_size) - itMax) + (buffer->endTw - buffer->data);
				}
			}	
		}

		//
		// Put character on buffer and if necessary transfer 
		// to destination file
		// 

		++tokensCount;
		addBuffer(codeAsPhrase, distance, matchesFound - 1, *buffer->endTw);
		

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

boolean 
__stdcall 
tryGetSourceFile(
	__in const char* fileName
) {
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
void 
__stdcall 
WriteLastByteIfNecessary() {

	if(freePtr != 0) {
		writeToFile();
	}
}

//
// This is the only method that write bufferToFile to destination file.
// Automatically cleanup the buffer for use the next data.
//
void 
__stdcall 
writeToFile() {

	fputc(fileChar, destination);			// Write character to destination
	fileChar = 0;							// Cleanup the buffer 
}

//
// After you have the destination file handle, before you start putting 
// data within, you must advance the pointer first.
// Typically this method should be called before you start compress anything only once.
// Before this space is the header of a pak file.
//
void
__stdcall 
positionFileDestinationPtrToWrite() {

	fseek(destination, 0, SEEK_SET);
	fseek(destination, sizeof(HeaderPak) + sourceFileExtensionSize, SEEK_SET); 
}

//
// This method set the destination file* global variable to file destination (pak)
// Return: true if file was created, and false if not.
//
boolean
__stdcall 
tryCreateDestinationFile(
	__in const char* filename
) {
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
		sourceFileExtension = NULL;									// Tells that source file haven't extension!	
	}
	else {

		//
		// File with extension
		// 

		sourceFileExtensionSize = strlen(ptrLastPoint) - 1;
		sourceFileExtension = (char*) malloc(sourceFileExtensionSize + 1);		// +1 for the \0 terminator
		strcpy(sourceFileExtension, ++ptrLastPoint);						// advance ptr to first char


		//

		copyTo = sourceLen - (sourceFileExtensionSize + 1);
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
void 
__stdcall 
doHeader() {

	PHeaderPak header = (PHeaderPak) malloc(sizeof(HeaderPak));

	header->id[0] = 'P';
	header->id[1] = 'A';
	header->id[2] = 'K';
	header->id[3] = '\0';

	header->offset = sizeof(HeaderPak) + sourceFileExtensionSize - HEADER_OFFSET;
	header->position_bits = twNecessaryBits;
	header->coincidences_bits = lhNecessaryBits;
	header->min_coincidences = 1;
	header->tokens = tokensCount;

	fseek(destination, 0, SEEK_SET);
	fwrite(header, sizeof(HeaderPak), 1, destination);
	fwrite(sourceFileExtension, sourceFileExtensionSize, 1, destination);

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

	if(argc < 4)
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

	doCompression(argv[2], argv[3]);

	doHeader();

	//
	// Close kernel handle to the file
	//


	fclose(source);
	fclose(destination);


	free(sourceFileExtension);

	return RETURN_OK;
}