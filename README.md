# strpcom
strip comments from C programs

Program in several successive stages of overhaul.
Intended to fulfill the challenge posted to comp.lang.c by Tim Rentsch on 5 Dec 2020.

The gg interface is abysmal at the moment, but here's a link to the thread.
https://groups.google.com/g/comp.lang.c/c/kYOAuWbYND0

The first version `strpcom.c` is a translation of a Haskell program and simulates 
lazy evaluation. It removes line continuation and then removes comments.
Further discussion in the newsgroup revealed that this was not the desired behavior
and line continuations should be preserved.

The second version `strpcom-v2.c` considers line continuations only while deleting 
the contents of single line comments. It doesn't see comment markers that are hidden
by line continuations.

The third version `strpcom-v3.c` introduces a pattern matching function `match()` 
but otherwise doesn't advance the situation wrt continuations and comments.

The final 2 versions actually solve the line continuation problem.

The fourth version `strpcom-v4.c` removes line continuations and counts them. Thus,
it sees through hidden comment markers. Then as a final pass, it uses the counts to
restore any continuations that survive comment pruning. It also adds a garbage 
collector.

The fifth version `strpcom-v5.c` removes the lazy evaluation and adds a configurable
final garbage collection pass. 

