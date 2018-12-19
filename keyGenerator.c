/*
 * Andrew Soback
 * Program 4 - OTP
 * CS 344
 */

/*
 * This Program takes an integer argument, 
 * and prints out a random sequence of characters of equal size
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

char getLetter(int);

int main(int argc, char* argv[]){
	if (argc != 2){
		printf("wrong num args %s\n", argc);
		return 1;
	}
	srand(time(0));

	char each;
	int sizeKey = atoi(argv[1]);
	int i;
	char buff[sizeKey + 1];
	memset(buff, '\0', sizeKey + 1);
	for (i = 0; i < sizeKey; i++){
		each = getLetter(rand());
		buff[i] = each;
	}	
	printf("%s\n", buff);
	return 0;
}

//This function takes a random integer, and converts it to a capital letter, or space
char getLetter(int x){
	x = x % 27;
	if (x == 0 ){
		return ' ';
	}
	x += 64;
	return (char)(x);
}
