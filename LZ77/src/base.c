#include "base.h"

void 
__stdcall
copyCharsAddExtension(
	__in char* destination, 
	__in char* source, 
	__in int nElems,
	__in char* extension
) {

	unsigned char extLength = strlen(extension);

	// Copy chars from source to destination
	for( ; nElems > 0; nElems--, destination++, source++) {
		*destination = *source;
	}

	// Add .
	*(destination)++ = '.';

	// Add extension
	for( ; extLength > 0; extLength--, destination++, extension++) {
		*destination = *extension;
	}

	// Finish string boundaries.
	*destination = '\0';
}

char*
__stdcall 
trySetDestinationFile(
	__in char* srcFilename,
	__in char* dstFileExt,
	__inout boolean* success
){
	unsigned char srcFilenameLength = strlen(srcFilename);
	unsigned char dstFileExtLength = strlen(dstFileExt);

	char* dotPtr = strrchr(srcFilename, '.');
	char* dstFilename;
	char* srcFilenameExt;

	if( dotPtr == NULL ) {

		//
		// source file don't contain any extension
		//

		dstFilename = (char*) malloc(srcFilenameLength + 2 + dstFileExtLength);		// +2 for . and '\0'
		copyCharsAddExtension(dstFilename, srcFilename, srcFilenameLength, dstFileExt);
		*srcFilenameExt = NULL;
	}

	else {
		
		unsigned char bodyLength = dotPtr - srcFilename;
		dstFilename = (char*) malloc(bodyLength + 2 + dstFileExtLength);		// +2 for . and '\0'
		copyCharsAddExtension(dstFilename, srcFilename, bodyLength, dstFileExt);
		
		// Alloc on heap the source extension (the caller must call free)
		srcFilenameExt = (char*) malloc(srcFilenameLength - (dotPtr - srcFilename));
		strcpy(srcFilenameExt, ++dotPtr);
	}

	destFile = fopen(dstFilename, "wb");

	free(dstFilename);
	*success = (boolean) destFile != NULL;

	if( !success && dotPtr != NULL ) {
		free(srcFilenameExt);
	}

	return srcFilenameExt;
}