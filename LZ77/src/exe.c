#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "CircularBuffer.h"


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
	strcpy(destination, ".pak\0");
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
		destLen = sourceLen + 5;						// Name of the file plus .pak\0
		originExtension = NULL;							// Tells that source file haven't extension!	
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
		destLen = copyTo + 5;		
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

	free(destFilename);									// Cleanup memory (we just need the file handle)

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
		return;
	}

	//
	// If we get here we have access to 2 files in order to apply to our algorithm.
	// 


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
	// Here we got the file destination and source handle
	// 
	

	// doCompression(sourceFile);


	//
	// Close kernel handle to the file
	//
	fclose(source);

	return 0;
}






