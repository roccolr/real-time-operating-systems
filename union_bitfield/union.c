#include <stdio.h>


union dato{
	int i;
	float f;
       	char c[4];
};


/* | I I I I | 
 * | F F F F |
 * | F F F F |
 * | C C C C |
 * */

int main(void){
	union dato d;
	d.i = 65;
	printf("sizeof (float) = %zu\n", sizeof(float));
	printf("sizeeof(union dato) = %zu\n", sizeof(union dato));
	printf("come int: %d\n", d.i);
	printf("come char: %c\n", d.c[0]);

	d.f= 3.14f;
	printf("dopo aver scritto f, i vale: %d\n", d.i);
	return 0;
}
