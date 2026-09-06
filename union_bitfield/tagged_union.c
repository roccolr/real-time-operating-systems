#include <stdio.h>


typedef enum {INTERO, REALE, TESTO } TipoValore;

typedef struct {
	TipoValore tipo;
	union {
		int i;
		double d;
		char* s;
	}valore;
}Valore;

void stampa(Valore v){
	switch(v.tipo){
		case INTERO: printf("int -> %d\n", v.valore.i); break;
		case REALE: printf("float -> %g\n", v.valore.d); break;
		case TESTO: printf("str -> %s\n", v.valore.s); break;
	}
}

int main(void){
	Valore a = { .tipo = INTERO, .valore.i = 42};
	Valore b = { .tipo = REALE, .valore.d = 3.14 };
	Valore c = { .tipo = TESTO, .valore.s = "hello"};
	stampa(a); stampa(b); stampa(c);
	return 0;
}	
