<!-- Project Ambrose by Imjustchico: What the fuzz seeds are and how to add one. -->

# data/fuzz

Seed inputs for the fuzzers the repository already builds. One folder per fuzzer, named for it, holding small files that made a decoder work hard.

A seed is bytes you produced yourself, from your own captures or your own generator. It is not a file from the game client, and it is not a whole capture: keep it to the smallest input that still exercises the path.
