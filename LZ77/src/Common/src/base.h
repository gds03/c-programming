#ifndef _BASE_H_
#define _BASE_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pak.h"


#define PAK_LENGTH 3
#define HEADER_OFFSET 5
#define CHAR_TOKEN_SIZE_BITS 9
#define CHAR_SIZE_BITS (sizeof(char) * 8)
#define INT_SIZE_BITS (sizeof(int) * 8)


typedef enum _boolean
{
	FALSE = 0,
	TRUE = 1
} boolean;

typedef enum applicationResults 
{
	ARGUMENTS_INVALID = 1,
	SOURCE_FILE_NOT_FOUND = 2,
	DEST_FILE_ERROR_CREATING = 3,
	RETURN_OK = 0
};


FILE* sourceFile;
char* sourceFileExtension;

FILE* destFile;
unsigned char fileChar;			// Character buffer to be written on the file
unsigned char freePtr;


unsigned int twNecessaryBits;
unsigned int lhNecessaryBits;
unsigned int phraseTokenBits;
unsigned long tokensCount;


//
// Returns one mask with all bits with 1 for given bits.
// E.g. bits = 7 returns 1111111 
// 
unsigned int  
__forceinline 
getNecessaryMaskFor(
	__in unsigned int bits
) {
	return (1 << bits) - 1;
}

//
// Given a number, returns the necessary bits to express the number
//
unsigned int 
__forceinline 
getNecessaryBitsFor(
	__in unsigned int value
) {
	unsigned int count = 0;

	while( value != 0 ) {
		value >>= 1;
		count++;
	}
	return count;
}


//
// Given two strings, copy from source to destination, 'nElems' characters
// and at the end, adds the extension passed by parameter
// 
void 
__stdcall
copyCharsAddExtension(
	__in char* source, 
	__in char* destination, 
	__in int nElems,
	__in char* extension
);


//
// This method set the sourceFile handler based on fileName.
// Return: true if the handler is returned, otherwise returns false.
// 
boolean
__forceinline 
trySetSourceFile(
	__in const char* fileName
) {
	sourceFile = fopen(fileName, "rb");
	return (boolean) sourceFile != NULL;
}

//
// After you have the destination file handle, before you start putting/reading the 
// data, you must advance the pointer first.
// Before this space is the header of a pak file.
//
void
__forceinline 
positionFileDestination(
	__in FILE* destination,
	__in unsigned char srcFileExtLength
) {
	fseek(destination, 0, SEEK_SET);
	fseek(destination, sizeof(HeaderPak) + srcFileExtLength, SEEK_SET); 
}

//
// This method set the destination file* to file destination (pak)
// Return: true if file was created, and false if not.
//
boolean
__stdcall 
tryCreateDestinationFile(
	__in char* srcFilename,
	__in char* dstFileExt
);


//
// This is the only method that write bufferToFile to destination file.
// Automatically cleanup the buffer.
//
void 
__forceinline 
writeToFile() {

	fputc(fileChar, destFile);				// Write character to destination
	fileChar = 0;							// Cleanup the buffer 
}




//
// This method verify if is data on the buffer, and if it is, write that data
// to destination file
// Typically is called once you have finish all compression process and
// has data on the buffer that must be flushed.
//
void 
__forceinline 
writeLastByteIfNecessary() {

	if( freePtr != 0 ) {
		writeToFile();
	}
}


#endif