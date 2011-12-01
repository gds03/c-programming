
/*
   O ficheiro compactado com extensão "pak" tem um cabeçalho (HdrPak) com informação adicional antes dos tokens.
   +----------------------------------------------------------+---------------------------------------
   |                  HdrPak                                  |      Tokens                        ...
   +----------------------------------------------------------+---------------------------------------
*/

struct HeaderPak {
	char id[4];						/* {'P','A','K',0} */
	unsigned char offset;			/* to data bits. 10 if EXT_CHAR==3 */
	unsigned char position_bits;	/* number of bits to position code */
	unsigned char coincidences_bits;/* number of bits to coincidences code */
	unsigned char min_coincidences; /* minimum of coincidences coded */
	unsigned long tokens;			/* total tokens */
	/*char extension[EXT_CHARS]*/   /* extension chars of original file */
};

typedef struct HeaderPak HdrPak;
/*
   O cabeçalho tem a estrtura definida em struct HeaderPak que pode ter o seguinte aspecto:
   +---+---+---+---+---+---+---+---+--------------+---+---+---+---------------------------------------
   |'P'|'A'|'K'| 0 | 10| 14| 4 | 3 |   534        |'t'|'x'|'t'|      Tokens                        ...
   +---+---+---+---+---+---+---+---+--------------+---+---+---+---------------------------------------
   - Os primeiros 4 bytes {'P','A','K',0} são uma marca para este tipo de ficheiros.
   - O quinto byte indica quanto bytes se salta no ficheiro para a zona dos tokens. 
   - O sexto byte é o número de bits utilizados para identificar a posição nos tokens de frase.
   - O sétimo byte é o número de bits utilizados para indicar o número de coincidências nos tokens de frase.
   - O oitavo byte é o número minimo de coincidêncas codificadas.
   - Do nono ao décimo segundo byte está um long (o primeiro byte é o de menor peso) com o número total de tokens no ficheiro.
   - Do décimo terceiro até aos tokens estão os caracteres da extensão original do ficheiro.
*/

/*
  Os tokens de simbolo são codificados com 9 bits, um zero seguindo dos 8 bits do simbolo: 0 s7 s6 s5 s4 s3 s2 s1 s0
  Os tokens de frase são codificados com (1+position_bits+coincidences_bits).
    Supondo position_bits=6 e coincidences_bits=3: 1 p5 p4 p3 p2 p1 p0 c2 c1 c0
*/
