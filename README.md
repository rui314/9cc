9cc C compiler
==============

Note: 9cc is no longer an active project, and the successor is
[chibicc](https://github.com/rui314/chibicc).

9cc is a successor of my [8cc](https://github.com/rui314/8cc) C compiler.
In this new project, I'm trying to write code that can be understood
extremely easily while creating a compiler that generates reasonably
efficient assembly.

9cc has more stages than 8cc. Here is an overview of the internals:

 1. Compiles an input string to abstract syntax trees.
 2. Runs a semantic analyzer on the trees to add a type to each tree node.
 3. Converts the trees to intermediate code (IR), which in some degree
    resembles x86-64 instructions but has an infinite number of registers.
 4. Maps an infinite number of registers to a finite number of registers.
 5. Generates x86-64 instructions from the IR.

There are a few important design choices that I made to keep the code as
simple as I can get:

 - Like 8cc, no memory management is the memory management policy in 9cc.
   We allocate memory using malloc() but never call free().
   I know that people find the policy odd, but this is actually a
   reasonable design choice for short-lived programs such as compilers.
   This policy greatly simplifies code and also eliminates use-after-free
   bugs entirely.

 - 9cc's parser is a hand-written recursive descendent parser, so that the
   compiler doesn't have any blackbox such as lex/yacc.

 - I stick with plain old tools such as Make or shell script so that you
   don't need to learn about new stuff other than the compiler source code
   itself.

 - We use brute force if it makes code simpler. We don't try too hard to
   implement sophisticated data structures to make the compiler run faster.
   If the performance becomes a problem, we can fix it at that moment.

 - Entire contents are loaded into memory at once if it makes code simpler.
   We don't use character IO to read from an input file; instead, we read
   an entire file to a char array in a batch. Likewise, we tokenize a
   whole file in a batch rather than doing it concurrently with the parser.

Overall, 9cc is still in its very early stage. I hope to continue
improving it to the point where 9cc can compile real-world C programs such
as Linux kernel. That is an ambitious goal, but I believe it's achievable,
so stay tuned!
