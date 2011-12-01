
/*
   O ficheiro compactado com extens�o "pak" tem um cabe�alho (HdrPak) com informa��o adicional antes dos tokens.
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
   O cabe�alho tem a estrtura definida em struct HeaderPak que pode ter o seguinte aspecto:
   +---+---+---+---+---+---+---+---+--------------+---+---+---+---------------------------------------
   |'P'|'A'|'K'| 0 | 10| 14| 4 | 3 |   534        |'t'|'x'|'t'|      Tokens                        ...
   +---+---+---+---+---+---+---+---+--------------+---+---+---+---------------------------------------
   - Os primeiros 4 bytes {'P','A','K',0} s�o uma marca para este tipo de ficheiros.
   - O quinto byte indica quanto bytes se salta no ficheiro para a zona dos tokens. 
   - O sexto byte � o n�mero de bits utilizados para identificar a posi��o nos tokens de frase.
   - O s�timo byte � o n�mero de bits utilizados para indicar o n�mero de coincid�ncias nos tokens de frase.
   - O oitavo byte � o n�mero minimo de coincid�ncas codificadas.
   - Do nono ao d�cimo segundo byte est� um long (o primeiro byte � o de menor peso) com o n�mero total de tokens no ficheiro.
   - Do d�cimo terceiro at� aos tokens est�o os caracteres da extens�o original do ficheiro.
*/

/*
  Os tokens de simbolo s�o codificados com 9 bits, um zero seguindo dos 8 bits do simbolo: 0 s7 s6 s5 s4 s3 s2 s1 s0
  Os tokens de frase s�o codificados com (1+position_bits+coincidences_bits).
    Supondo position_bits=6 e coincidences_bits=3: 1 p5 p4 p3 p2 p1 p0 c2 c1 c0
*/
