How to compile:
===============

There is only one *.c file and two *.h files, with a completely flat directory
structure, so compiling is very straightforward:

$ gcc -o smallsh comitoz.smallsh.c

For debugging purposes I usually use

$ gcc -o smallsh comitoz.smallsh.c -O -g -ftrapv

...plus warning flags.

===============

Comments are all in Markdown
(https://github.com/adam-p/markdown-here/wiki/Markdown-Cheatsheet) format.
