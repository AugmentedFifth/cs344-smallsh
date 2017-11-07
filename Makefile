comitoz.smallsh: comitoz.utils.h comitoz.smallsh.h comitoz.smallsh.c
	gcc -o smallsh comitoz.smallsh.c -O -g -ftrapv -Wall -Wextra -Wshadow -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wformat=2 -Winit-self
