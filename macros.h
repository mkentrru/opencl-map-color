#include <stdio.h>

#define check(COND, MSG, CODE) { if(COND) {\
printf("\n\t< %s (%d).\n", MSG, CODE); return EXIT_FAILURE;\
}\
}

#define check_goto(COND, MSG, CODE, LABLE) { if(COND) {\
printf("\n\t< %s (%d).\n", MSG, CODE); goto LABLE;\
}\
}

#define check_goto_temp(COND, MSG, CODE) { if(COND) {\
printf("\n\t< %s (%d).\n", MSG, CODE); callres = EXIT_FAILURE; goto free_temporary_resources;\
}\
}

#define temp {goto free_temporary_resources;}