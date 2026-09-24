<!-- Project Ambrose by Imjustchico: How a contributed tool is laid out and what it may assume. -->

# contrib/tools

One folder per tool, named for what it does. Inside it: the code, its own dependency list, and a README saying what it is for, how to run it, what it was run against and what it printed.

Rules: it reads a user's own installation or an Ambrose server at run time and writes only its own output; it never writes inside the game install; it never commits a game file or anything generated from one that carries its content; it adds nothing to the repository's manifests; and every file carries the Project Ambrose branding header and its one-line brief, with no other comments.
