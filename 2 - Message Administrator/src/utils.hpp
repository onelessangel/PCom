#ifndef _HELPERS_HPP
#define _HELPERS_HPP 1

#include <iostream>

#define BUF_LEN 1600
#define EXIT "exit"

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#endif
