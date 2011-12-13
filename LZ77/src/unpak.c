#include "base.h"
#include "circularBuffer.h"



boolean verifyPakHeader() {
	char shouldBePakExtension[4];

	fseek(sourceFile, 0, SEEK_SET);
	fread(shouldBePakExtension, 1, 4, sourceFile);

	return (boolean) (shouldBePakExtension[0] == 'P' && shouldBePakExtension[1] == 'A' &&
					 shouldBePakExtension[2] == 'K' && shouldBePakExtension[3] == '\0');
}

void setSourceFileInformation() {
	unsigned char srcExtLength, offset;
	
	fread(&offset, 1, member_size(HeaderPak, offset), sourceFile); 
	sourceFileExtension = (char*) malloc( (srcExtLength = (offset - after_offset_size)) + 1 );		// +1 for \0 character

	fread(&twNecessaryBits, 1, member_size(HeaderPak, position_bits), sourceFile);
	fread(&lhNecessaryBits, 1, member_size(HeaderPak, coincidences_bits), sourceFile);

	fseek(sourceFile, 1, SEEK_CUR);			// Advance 1 byte (ignore min_coincidences)
	fread(&tokensCount, 1, member_size(HeaderPak, tokens), sourceFile);

	fread(sourceFileExtension, 1, srcExtLength, sourceFile);	// Set extension on sourceFileExtension
	*(sourceFileExtension + srcExtLength) = '\0';				// End the string

	phraseTokenBits = twNecessaryBits + lhNecessaryBits + 1;
}

char* getItMax(PRingBufferChar buffer, unsigned int distance) {

	if( buffer->finish > buffer->start ) {
		return buffer->finish - distance;
	}

	return ((buffer->finish - distance) >= buffer->data)
		   ? (buffer->finish - distance)
		   : (buffer->data + buffer->buffer_size) - (distance - (buffer->finish - buffer->data));
}

void doDecompression() {

	PRingBufferChar buffer = newInstance(
		getNecessaryMaskFor(twNecessaryBits), 
		getNecessaryMaskFor(lhNecessaryBits) + 1	// Ignored
	);

	int c = fgetc(sourceFile);
	boolean isCharacter = (boolean) (c & (1 << (CHAR_SIZE_BITS - 1)) == 0);

	//
	// 1st Stage: Process & Decompress
	//

	while( c != EOF ) 
	{
		unsigned char ch = c;		// Used for safety shifts

		if( isCharacter ) {
			unsigned char theChar = 0;
			unsigned char shifts  = 0;			
			
			// +1 for 0 bit token
			theChar = ( ch & getNecessaryMaskFor( CHAR_SIZE_BITS - (shifts = freePtr + 1) ) ) << shifts;
			c = fgetc(sourceFile);
			
			if( c == EOF ) break;
			else {
				ch = c;
				theChar |= ch >> (CHAR_SIZE_BITS - shifts);
				putTextWindow(buffer, theChar);			// Put character on dictionary and write to destination
				fputc(theChar, destFile);

				if( freePtr == (CHAR_SIZE_BITS - 1) ) {

					// When this happends, we need to load another byte from source, to continue processing
					c = fgetc(sourceFile);
					if( c == EOF ) break;
				}
			}

			freePtr = (freePtr + CHAR_TOKEN_SIZE_BITS) % CHAR_SIZE_BITS;
		}

		else {
			unsigned int distance, occurrences;
			unsigned char remainingDistanceBits = twNecessaryBits;
			unsigned char remainingOccurrencesBits = lhNecessaryBits;

			char* itMax;



			


			//
			// After we got the distance and occurrences (dificult part), we can 
			// start filling the dictionary

			itMax = getItMax(buffer, distance);

			for( ; occurrences > 0; occurrences--) {
				char character = *itMax;
				incrementPtr(buffer, itMax)

				putTextWindow(buffer, character);
				fputc(character, destFile);
			}

			freePtr = (freePtr + phraseTokenBits) % CHAR_SIZE_BITS;
		}

		isCharacter = (boolean) (c & (1 << (CHAR_SIZE_BITS - 1 + freePtr)) == 0);
	}
}


int main(int argc, char* argv[]) {

	boolean success;

	if( argc != 2 ) {
		fprintf(stderr, "Arguments invalid: Should be unpak <filename>");
		return ARGUMENTS_INVALID;
	}

	if( !trySetSourceFile(argv[1]) ) {
		fprintf(stderr, "File to decompress not found \n");
		return SOURCE_FILE_NOT_FOUND;
	}

	//
	// Before we can create the destination file,
	// we must know what's the extension of original file (we must read the header)
	//

	if( !verifyPakHeader() ) {
		fprintf(stderr, "File to decompress is not a pak file \n");
		return SOURCE_FILE_NOT_PAK_FILE;
	}

	setSourceFileInformation();

	//
	// At this point the source file ptr is pointing to the first token (1st byte)
	// 

	sourceFileExtension = trySetDestinationFile(argv[1], sourceFileExtension, &success);

	if( !success ) {
		fprintf(stderr, "Error while creating decompressed file \n");
		return DEST_FILE_ERROR_CREATING;
	}



	doDecompression();


	//
	// Close kernel handle to the file
	//


	fclose(sourceFile);
	fclose(destFile);


	free(sourceFileExtension);

	return RETURN_OK;

}