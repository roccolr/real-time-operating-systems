#include <stdio.h>
#include <stdint.h>

typedef union {
	uint8_t raw; // byte intero grezzo
	struct {
		uint8_t enable 	:	1; // bit 0
		uint8_t mode	:	2; // bit 1,2
		uint8_t prio	:	3; // bit 3,4,5
		uint8_t reserv	:	2; // bit 6,7
	}bits;	
} ctrl_register;

int main(void){
	ctrl_register reg;
	reg.raw = 0; // set to 0
	reg.bits.enable = 1; // 0b1
	reg.bits.mode = 2; // 0b10
	reg.bits.prio = 5; // 0b101
	
	printf(" byte raw: 0x%02X\n", reg.raw); // 0b00101101 -> 0x2D
	reg.raw = 0xFF;
	printf( "mode -> %u\n", reg.bits.mode);
	printf("priority -> %u\n", reg.bits.prio);
	return 0;
}
