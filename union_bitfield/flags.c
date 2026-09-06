#include <stdio.h>

struct permessi{
	unsigned char lettura 	:1;
	unsigned char scrittura	:1;
	unsigned char esecuzione	:1;
	unsigned char 		:5; //completamento del byte
};

int main(void){
	struct permessi p ={	.lettura = 1, 
				.scrittura = 0,
				.esecuzione = 1
				};

	printf("dimensione pacchetto -> %zu\n", sizeof(p));
	return 0;
}
