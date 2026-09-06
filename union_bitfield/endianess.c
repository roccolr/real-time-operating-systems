#include <stdio.h>
#include <stdint.h>


int main(void){
	union {
		uint32_t value;
		uint8_t bytes[4];
	} u;
	u.value = 0x01020304;
	printf("byte 0 = 0x%02X\n", u.bytes[0]);
	/* little endian -> 0x04 
	 * big endian -> 0x01
	 */

	return 0;
}
