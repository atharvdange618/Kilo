kilo: kilo.c
		$(CC) kilo.c -o kilo -Wall -Wextra -pedantic -std=c99

# first line says that kilo is what we want to build, and that kilo.c is what's required to build it.
# the second line specifies the command to run in order to actually build kilo out kilo.c99
# Note: all makefiles must use tabs
#
# few things that are added to the compilation command
# $(CC) is a variable that make expands to cc by default
# -Wall stands for "all warnings", and gets the compiler to warn you when it sees
# code in your program that might not technically be wrong, but is considered bad or questionable usage of C language
#	like using variables before initializing them.
#	-Wextra and -pedantic turn on even more warnings.
#	-std=c99 specifies the exact version of C language standard we're using,
#	which is C99. C99 allows us to declare variables to be declare variables anywhere within a function,
#	where as ANSI C requires all variables to be declared at the top of a function or block
