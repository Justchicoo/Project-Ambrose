# Project Ambrose by Imjustchico
# Prints a Markdown file's heading index with section sizes, or only the sections whose heading matches a pattern.
import argparse
import re
import sys

HEADING = re.compile(r"(#{1,6}) (.*)")


def headings(lines):
    found = []
    fenced = False
    for index, line in enumerate(lines):
        if re.match(r" {0,3}(```|~~~)", line):
            fenced = not fenced
            continue
        match = None if fenced else HEADING.fullmatch(line)
        if match:
            found.append((index, len(match.group(1)), match.group(2)))
    return found


def section_end(lines, found, position):
    level = found[position][1]
    for index, other, _ in found[position + 1:]:
        if other <= level:
            return index
    return len(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Read a Markdown file by its sections instead of whole.")
    parser.add_argument("file")
    parser.add_argument("pattern", nargs="?", help="case-insensitive regular expression matched against heading text")
    parser.add_argument("--depth", type=int, default=3, help="deepest heading level the index lists")
    parser.add_argument("--first", action="store_true", help="print only the first matching section")
    args = parser.parse_args(argv)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    with open(args.file, encoding="utf-8") as handle:
        lines = handle.read().split("\n")
    found = headings(lines)
    if args.pattern is None:
        for position, (index, level, text) in enumerate(found):
            if level <= args.depth:
                size = sum(len(line) + 1 for line in lines[index:section_end(lines, found, position)])
                print(f"{index + 1:>6} {'  ' * (level - 1)}{'#' * level} {text}  [{size // 1024} KB]" if size >= 1024 else f"{index + 1:>6} {'  ' * (level - 1)}{'#' * level} {text}")
        return 0
    wanted = re.compile(args.pattern, re.IGNORECASE)
    printed = 0
    for position, (index, _, text) in enumerate(found):
        if wanted.search(text):
            print(f"[{args.file}:{index + 1}]")
            print("\n".join(lines[index:section_end(lines, found, position)]).rstrip("\n"))
            print()
            printed += 1
            if args.first:
                break
    if not printed:
        print(f"no heading in {args.file} matches {args.pattern!r}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
