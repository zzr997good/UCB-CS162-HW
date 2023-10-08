#include <stdio.h>
#include <stdlib.h>

/* A statically allocated variable */
int foo;

/* UNCOMMENT THIS LINE for 3.4.3*/
extern int recur(int i);


/* A statically allocated, pre-initialized variable */
volatile int stuff = 7;

int main(int argc, char *argv[]) {
    /* A stack allocated variable */
    volatile int i = 0;

    /* Dynamically allocate some stuff */
    volatile char *buf1 = malloc(100);
    /* ... and some more stuff */
    volatile char *buf2 = malloc(100);

    recur(3);
    return 0;
}

// Run GDB on the map executable. If you are using the Workspace please use i386-gdb.
// $i386-gdb map

// Set a breakpoint at the beginning of the program’s execution.
// (gdb) b 15

// Continue the program until the breakpoint.
// (gdb) c

// What memory address does argv store?
// (gdb) p/x argv

// Describe what’s located at that memory address. (What does argv point to?)
// (gdb) p *argv

// Step until you reach the first call to recur.
// (gdb) n

// What is the memory address of the recur function?
// (gdb) info address recur

// Step into the first call to recur.
// (gdb) n

// Step until you reach the if statement.
// (gdb) s

// Switch into assembly view.
// (gdb) layout asm

// Step over instructions until you reach the callq instruction (or the call instruction if you are using QEMU).
// (gdb) s

// What values are in all the registers?
// (gdb) info registers

// Step into the callq instruction.
// (gdb) s

// Switch back to C code mode.
// (gdb) layout src

// Now print out the current call stack. Hint: what does the backtrace command do?
// (gdb) x/40x $sp

// Now set a breakpoint on the recur function which is only triggered when the argument is 0.
// (gdb) b recurse:4 
// (gdb) condition 1 i==0

// Continue until the breakpoint is hit.
// (gdb) c

// Print the call stack now.
// (gdb) bt

// Now go up the call stack until you reach main. What was argc?
// 0x565ff23f in main (argc=1, argv=0xfff26be4) at map.c:23

// Now step until the return statement in recur.
// (gdb) s

// Switch back into the assembly view.
// (gdb) layout a

// Which instructions correspond to the return 0 in C?
// (gdb) mov $0x0,%eax

// Now switch back to the source layout.
// (gdb) layout src

// Finish the remaining 3 function calls.
// (gdb) c

// Run the program to completion.
// (gdb) c

// Quit GDB.
// (gdb) q