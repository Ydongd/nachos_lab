/* halt.c
 *	Simple program to test whether running a user program works.
 *	
 *	Just do a "syscall" that shuts down the OS.
 *
 * 	NOTE: for some reason, user programs with global data structures 
 *	sometimes haven't worked in the Nachos environment.  So be careful
 *	out there!  One option is to allocate data structures as 
 * 	automatics within a procedure, but if you do this, you have to
 *	be careful to allocate a big enough stack to hold the automatics!
 */

#include "syscall.h"

/*int fd1, fd2;
char *buffer;*/
int id;

void ff(){
    Exit(2);
}
void f(){
    Exit(3);
}

int
main()
{
    id = Exec("../test/test");
    Join(id);
    Fork(ff);
    Yield();
    Fork(f);
    
    //Exec("../test/sort");
    
    /*Create("11.txt");
    
    fd1 = Open("11.txt");
    fd2 = Open("22.txt");
    
    Read(buffer, 18, fd2);
    Write(buffer, 18, fd1);
    
    Close(fd1);
    Close(fd2);*/
    
    Exit(0);
    /* not reached */
}
