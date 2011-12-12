#include "base.h"

void 
__stdcall
copyCharsAddExtension(
	__in char* source, 
	__in char* destination, 
	__in int nElems,
	__in char* extension
) {

	//
	// Copy chars from source to destination
	for( ; nElems > 0; nElems--, destination++, source++) {
		*destination = *source;
	}

	// 
	// Add extension to destination
	strcpy(destination, extension);
}

boolean
__stdcall 
tryCreateDestinationFile(
	__in char* srcFilename,
	__in char* dstFileExt
) {
	//
	// 'example.xslt' -> 'example.pak' | 'example' -> 'example.pak'
	// 
	
	char* dstFilename;
	int dstLength, charsToCopy;

	int srcLength				   = strlen(srcFilename);	
	char* ptrLastPoint			   = strrchr(srcFilename, '.');
	unsigned char dstFileExtLength = strlen(dstFileExt) - 1;	// Exclude the .
	
	
	if( ptrLastPoint == NULL ) {

		//
		// File without extension
		// 

		charsToCopy = srcLength;						// Copy all characters
		dstLength = srcLength + dstFileExtLength + 2;	// +2 for . and for \0 characters
		sourceFileExtension = NULL;						// Indicate that the file have'nt extension			
	}
	else {

		//
		// File with extension
		// 

		unsigned char srcFileExtLength = strlen(++ptrLastPoint);		// ex: .txt returns 3

		sourceFileExtension = (char*) malloc(srcFileExtLength + 1);		// +1 for \0 character
		strcpy(sourceFileExtension, ptrLastPoint);						// Copy extension and \0 characters				

		//

		charsToCopy = srcLength - srcFileExtLength - 1;					// -1 for excluding the .
		dstLength = charsToCopy + dstFileExtLength + 2;					
	}

	//
	// Build destination name
	// 	
	dstFilename = (char*) malloc(dstLength);												// Heap alloc
	copyCharsAddExtension(srcFilename, dstFilename, charsToCopy, dstFileExt);


	//
	// Try open the file for writing..
	// 
	destFile = fopen(dstFilename, "wb");

	//
	// We don't need destination filename after this instruction, so we can free the memory!
	// 
	free(dstFilename);													// Heap free

	return (boolean) destFile != NULL;
}