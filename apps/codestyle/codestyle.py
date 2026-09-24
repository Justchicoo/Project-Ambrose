# Project Ambrose by Imjustchico
# Enforces the branding header, the no-comments rule, line endings, whitespace, and include guards.
import argparse
import io
import os
import re
import subprocess
import sys
import tokenize
from dataclasses import dataclass

BRAND = "Project Ambrose by Imjustchico"

CPP_EXTENSIONS = {".h", ".hh", ".hpp", ".c", ".cc", ".cpp", ".cxx", ".inl", ".ipp"}
HEADER_EXTENSIONS = {".h", ".hh", ".hpp"}
WEBCODE_EXTENSIONS = {".ts", ".tsx", ".js", ".jsx", ".mjs", ".cjs", ".css"}
BINARY_EXTENSIONS = {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".woff", ".woff2", ".ttf", ".otf", ".svg", ".bin"}
CRLF_KINDS = {"batch", "powershell"}
HASH_KINDS = {"cmake", "shell", "powershell", "python", "yaml", "linehash", "editorconfig", "conf"}
BLOCK_KINDS = {"cpp", "webcode"}
MARKUP_KINDS = {"markdown", "svelte", "html"}


@dataclass(frozen=True)
class Issue:
    path: str
    line: int
    rule: str
    message: str

    def __str__(self):
        return f"{self.path}:{self.line}: {self.rule}: {self.message}"


def classify(relpath):
    name = relpath.rsplit("/", 1)[-1]
    if name.endswith(".in"):
        name = name[:-3]
    if name.endswith(".dist") and not name.lower().endswith(".conf.dist"):
        name = name[:-5]
    lower = name.lower()
    if name == "CMakeLists.txt":
        return "cmake"
    if lower in ("env", ".env") or lower.endswith(".env"):
        return "conf"
    if lower in (".gitignore", ".gitattributes", ".gitkeep"):
        return "linehash"
    if lower in ("license", "license.txt", "notice"):
        return "exempt"
    if lower == ".editorconfig":
        return "editorconfig"
    if lower.endswith(".conf.dist") or lower.endswith(".conf"):
        return "conf"
    if lower in (".npmrc", ".nvmrc", ".prettierignore", ".eslintignore"):
        return "linehash"
    ext = os.path.splitext(lower)[1]
    if ext in CPP_EXTENSIONS:
        return "cpp"
    if ext in WEBCODE_EXTENSIONS:
        return "webcode"
    mapping = {
        ".svelte": "svelte",
        ".html": "html",
        ".cmake": "cmake",
        ".sh": "shell",
        ".ps1": "powershell",
        ".psm1": "powershell",
        ".psd1": "powershell",
        ".py": "python",
        ".yml": "yaml",
        ".yaml": "yaml",
        ".sql": "sql",
        ".bat": "batch",
        ".cmd": "batch",
        ".md": "markdown",
        ".txt": "linehash",
        ".json": "exempt",
    }
    if ext in mapping:
        return mapping[ext]
    if ext in BINARY_EXTENSIONS:
        return "exempt"
    return None


def brief_problem(brief):
    text = brief.strip()
    if len(text) < 3:
        return "the brief is empty or too short"
    if text.startswith("Project Ambrose"):
        return "the brief repeats the branding instead of describing the file"
    return None


def cpp_brief_problem(brief):
    if "/*" in brief or "*/" in brief:
        return "the brief holds '/*' or '*/', which nests in or ends the header comment"
    return brief_problem(brief)


def check_header(kind, lines):
    issues = []
    start = 0
    if kind in ("shell", "python") and lines and lines[0].startswith("#!"):
        start = 1
    if kind == "batch" and lines and lines[0].strip().lower() == "@echo off":
        start = 1

    def line_at(index):
        return lines[index] if index < len(lines) else None

    if kind in BLOCK_KINDS:
        expected = ["/*", f" * {BRAND}", None, " */"]
        for offset, want in enumerate(expected):
            got = line_at(offset)
            if want is None:
                match = re.fullmatch(r" \* (.*)", got or "")
                if not match:
                    issues.append((offset + 1, "expected ' * <brief>' as the third header line"))
                else:
                    problem = cpp_brief_problem(match.group(1))
                    if problem:
                        issues.append((offset + 1, problem))
            elif got != want:
                issues.append((offset + 1, f"expected {want!r}"))
        if not issues:
            return 4, []
        skip = 0
        if line_at(0) == "/*":
            for index in range(1, min(len(lines), 6)):
                if lines[index].strip() == "*/":
                    skip = index + 1
                    break
        return skip, issues[:1]

    if kind in MARKUP_KINDS:
        match = re.fullmatch(r"<!-- " + re.escape(BRAND) + r": (.*) -->", line_at(0) or "")
        if not match:
            return (1 if (line_at(0) or "").startswith("<!--") else 0), [(1, f"expected '<!-- {BRAND}: <brief> -->'")]
        problem = brief_problem(match.group(1)) or ("the brief holds '-->', which ends the header comment" if "-->" in match.group(1) else None)
        return 1, ([(1, problem)] if problem else [])

    marker = {"sql": "--", "batch": "REM"}.get(kind, "#")
    first = line_at(start)
    second = line_at(start + 1)
    if first != f"{marker} {BRAND}":
        issues.append((start + 1, f"expected {marker + ' ' + BRAND!r}"))
    match = re.fullmatch(re.escape(marker) + r" (.*)", second or "")
    if not match:
        issues.append((start + 2, f"expected '{marker} <brief>' as the second header line"))
    else:
        problem = brief_problem(match.group(1))
        if problem:
            issues.append((start + 2, problem))
    if not issues:
        return start + 2, []
    looks_like_header = all((line_at(start + k) or "").startswith(marker) for k in range(2))
    return (start + 2 if looks_like_header else start), issues[:1]


def line_number_of(text, index):
    return text.count("\n", 0, index) + 1


def scan_cpp(text):
    hits = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] in "/*":
            hits.append(line_number_of(text, i))
            if text[i + 1] == "/":
                end = text.find("\n", i)
                i = n if end < 0 else end
            else:
                end = text.find("*/", i + 2)
                i = n if end < 0 else end + 2
            continue
        if c == '"':
            k = i
            while k > 0 and (text[k - 1].isalnum() or text[k - 1] == "_"):
                k -= 1
            prefix = text[k:i]
            if prefix in ("R", "u8R", "uR", "UR", "LR"):
                open_paren = text.find("(", i + 1)
                delimiter = text[i + 1:open_paren] if open_paren >= 0 else ""
                if open_paren >= 0 and len(delimiter) <= 16 and not re.search(r"[\s\\)]", delimiter):
                    end = text.find(")" + delimiter + '"', open_paren + 1)
                    i = n if end < 0 else end + len(delimiter) + 2
                    continue
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] in '"\n':
                    i += 1
                    break
                i += 1
            continue
        if c == "'":
            k = i
            while k > 0 and (text[k - 1].isalnum() or text[k - 1] in "_'."):
                k -= 1
            token = text[k:i]
            if token and token[0].isdigit() and i + 1 < n and text[i + 1].isalnum():
                i += 1
                continue
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] in "'\n":
                    i += 1
                    break
                i += 1
            continue
        i += 1
    return hits


def scan_python(text):
    hits = []
    try:
        for token in tokenize.generate_tokens(io.StringIO(text).readline):
            if token.type == tokenize.COMMENT:
                hits.append(token.start[0])
    except (tokenize.TokenError, IndentationError, SyntaxError) as error:
        return hits, str(error)
    return hits, None


def scan_cmake(text):
    hits = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == "\\":
            i += 2
            continue
        if c == '"':
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == '"':
                    i += 1
                    break
                i += 1
            continue
        if c == "[" and (i == 0 or text[i - 1] in " \t\n("):
            match = re.match(r"\[(=*)\[", text[i:])
            if match:
                end = text.find("]" + match.group(1) + "]", i + match.end())
                i = n if end < 0 else end + len(match.group(1)) + 2
                continue
        if c == "#":
            hits.append(line_number_of(text, i))
            match = re.match(r"#\[(=*)\[", text[i:])
            if match:
                end = text.find("]" + match.group(1) + "]", i + match.end())
                i = n if end < 0 else end + len(match.group(1)) + 2
            else:
                end = text.find("\n", i)
                i = n if end < 0 else end
            continue
        i += 1
    return hits


def scan_shell(text):
    hits = []
    i = 0
    n = len(text)
    pending = []
    while i < n:
        c = text[i]
        if c == "\n":
            i += 1
            while pending:
                delimiter, strip_tabs = pending.pop(0)
                while i < n:
                    end = text.find("\n", i)
                    body = text[i:n if end < 0 else end]
                    i = n if end < 0 else end + 1
                    if (body.lstrip("\t") if strip_tabs else body) == delimiter:
                        break
            continue
        if c == "\\":
            i += 2
            continue
        if c == "'":
            end = text.find("'", i + 1)
            i = n if end < 0 else end + 1
            continue
        if c == '"':
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == '"':
                    i += 1
                    break
                i += 1
            continue
        if text.startswith("<<", i) and not text.startswith("<<<", i):
            match = re.match(r"<<(-?)[ \t]*(['\"]?)([A-Za-z_][A-Za-z0-9_]*)\2", text[i:])
            if match:
                pending.append((match.group(3), match.group(1) == "-"))
                i += match.end()
                continue
        if c == "#":
            previous = text[i - 1] if i > 0 else "\n"
            if previous in " \t\n;&|(":
                hits.append(line_number_of(text, i))
                end = text.find("\n", i)
                i = n if end < 0 else end
                continue
        i += 1
    return hits


def scan_powershell(text):
    hits = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == "`":
            i += 2
            continue
        if c == "@" and i + 1 < n and text[i + 1] in "'\"" and text[i + 2:i + 3] in ("\n", "\r"):
            closer = "\n" + text[i + 1] + "@"
            end = text.find(closer, i + 2)
            i = n if end < 0 else end + len(closer)
            continue
        if c == "'":
            i += 1
            while i < n:
                if text[i] == "'" and text[i + 1:i + 2] == "'":
                    i += 2
                    continue
                if text[i] == "'":
                    i += 1
                    break
                i += 1
            continue
        if c == '"':
            i += 1
            while i < n:
                if text[i] == "`":
                    i += 2
                    continue
                if text[i] == '"':
                    i += 1
                    break
                i += 1
            continue
        if text.startswith("<#", i):
            hits.append(line_number_of(text, i))
            end = text.find("#>", i + 2)
            i = n if end < 0 else end + 2
            continue
        if c == "#":
            previous = text[i - 1] if i > 0 else "\n"
            if previous in " \t\n;|({}":
                hits.append(line_number_of(text, i))
                end = text.find("\n", i)
                i = n if end < 0 else end
                continue
        i += 1
    return hits


def scan_yaml(text):
    hits = []
    for number, line in enumerate(text.split("\n"), start=1):
        quote = None
        for index, char in enumerate(line):
            if quote:
                if char == quote:
                    quote = None
                continue
            if char in "'\"":
                quote = char
                continue
            if char == "#" and (index == 0 or line[index - 1] in " \t"):
                hits.append(number)
                break
    return hits


def scan_line_start(text, markers):
    return [number for number, line in enumerate(text.split("\n"), start=1) if line.lstrip().startswith(markers)]


def scan_sql(text):
    hits = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c in "'\"`":
            i += 1
            while i < n:
                if text[i] == "\\" and c != "`":
                    i += 2
                    continue
                if text[i] == c:
                    if text[i + 1:i + 2] == c:
                        i += 2
                        continue
                    i += 1
                    break
                i += 1
            continue
        if text.startswith("--", i) and text[i + 2:i + 3] in ("", " ", "\t", "\n", "\r"):
            hits.append(line_number_of(text, i))
            end = text.find("\n", i)
            i = n if end < 0 else end
            continue
        if c == "#":
            hits.append(line_number_of(text, i))
            end = text.find("\n", i)
            i = n if end < 0 else end
            continue
        if text.startswith("/*", i):
            hits.append(line_number_of(text, i))
            end = text.find("*/", i + 2)
            i = n if end < 0 else end + 2
            continue
        i += 1
    return hits


def scan_batch(text):
    hits = []
    for number, line in enumerate(text.split("\n"), start=1):
        stripped = line.strip().lstrip("@").lstrip()
        upper = stripped.upper()
        if upper.startswith("REM") and (len(stripped) == 3 or stripped[3] in " \t\r"):
            hits.append(number)
        elif stripped.startswith("::"):
            hits.append(number)
        elif re.search(r"&\s*rem(\s|$)", line, re.IGNORECASE):
            hits.append(number)
    return hits


def scan_markdown(text):
    hits = []
    in_fence = False
    for number, line in enumerate(text.split("\n"), start=1):
        if re.match(r" {0,3}(```|~~~)", line):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        if "<!--" in re.sub(r"`[^`]*`", "", line):
            hits.append(number)
    return hits


def skip_template(text, index):
    index += 1
    depth = 0
    while index < len(text):
        char = text[index]
        if char == "\\":
            index += 2
            continue
        if depth == 0 and char == "`":
            return index + 1
        if depth == 0 and text.startswith("${", index):
            depth = 1
            index += 2
            continue
        if depth > 0:
            if char == "`":
                index = skip_template(text, index)
                continue
            if char in "\"'":
                index += 1
                while index < len(text):
                    if text[index] == "\\":
                        index += 2
                        continue
                    if text[index] == char or text[index] == "\n":
                        index += 1
                        break
                    index += 1
                continue
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
        index += 1
    return len(text)


def scan_webcode(text):
    hits = []
    pieces = []
    index = 0
    start = 0
    while index < len(text):
        char = text[index]
        if char == "\\":
            index += 2
            continue
        if char in "\"'":
            index += 1
            while index < len(text):
                if text[index] == "\\":
                    index += 2
                    continue
                if text[index] == char or text[index] == "\n":
                    index += 1
                    break
                index += 1
            continue
        if char == "`":
            pieces.append((start, text[start:index]))
            index = skip_template(text, index)
            start = index
            continue
        index += 1
    pieces.append((start, text[start:]))
    for offset, piece in pieces:
        line = line_number_of(text, offset) - 1
        hits.extend(number + line for number in scan_cpp(piece))
    return hits


def scan_markup_comments(text):
    hits = []
    index = 0
    while True:
        index = text.find("<!--", index)
        if index < 0:
            return hits
        hits.append(line_number_of(text, index))
        end = text.find("-->", index + 4)
        index = len(text) if end < 0 else end + 3


def scan_svelte(text):
    hits = []
    cursor = 0
    markup = []
    lower = text.lower()
    while cursor < len(text):
        opening = -1
        tag = None
        for candidate in ("<script", "<style"):
            found = lower.find(candidate, cursor)
            if found >= 0 and (opening < 0 or found < opening):
                opening, tag = found, candidate[1:]
        if opening < 0:
            markup.append((cursor, text[cursor:]))
            break
        markup.append((cursor, text[cursor:opening]))
        body_start = text.find(">", opening)
        if body_start < 0:
            break
        closing = lower.find(f"</{tag}", body_start)
        body_end = len(text) if closing < 0 else closing
        block = text[body_start + 1:body_end]
        offset = line_number_of(text, body_start + 1) - 1
        hits.extend(number + offset for number in scan_webcode(block))
        cursor = body_end
    for start, chunk in markup:
        offset = line_number_of(text, start) - 1
        hits.extend(number + offset for number in scan_markup_comments(chunk))
    return hits


def check_include_guard(relpath, lines, skip):
    name = relpath.rsplit("/", 1)[-1]
    if name.endswith(".in"):
        name = name[:-3]
    stem = os.path.splitext(name)[0]
    guard = "AMBROSE_" + re.sub(r"[^A-Za-z0-9]", "_", stem).upper() + "_H"
    directives = [(index + 1, line.strip()) for index, line in enumerate(lines) if index >= skip and line.strip().startswith("#")]
    if len(directives) < 3 or directives[0][1] != f"#ifndef {guard}" or directives[1][1] != f"#define {guard}":
        line = directives[0][0] if directives else skip + 1
        return [(line, f"expected '#ifndef {guard}' followed by '#define {guard}'")]
    content = [(index + 1, line.strip()) for index, line in enumerate(lines) if line.strip()]
    if not content or content[-1][1] != "#endif":
        return [(content[-1][0] if content else 1, "expected the file to end with '#endif'")]
    return []


def check_file(relpath, raw):
    relpath = relpath.replace("\\", "/")
    name = relpath.rsplit("/", 1)[-1]
    kind = classify(relpath)
    if raw == b"" and name == ".gitkeep":
        return []
    if kind is None:
        return [Issue(relpath, 1, "unknown-type", "no codestyle rule covers this file type; add one to apps/codestyle/codestyle.py")]
    if kind == "exempt":
        return []
    issues = []
    if raw.startswith(b"\xef\xbb\xbf"):
        issues.append(Issue(relpath, 1, "encoding", "remove the UTF-8 byte order mark"))
        raw = raw[3:]
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as error:
        return issues + [Issue(relpath, 1, "encoding", f"file is not valid UTF-8: {error}")]
    if kind not in CRLF_KINDS and "\r" in text:
        issues.append(Issue(relpath, line_number_of(text, text.index("\r")), "line-ending", "use LF line endings"))
    text = text.replace("\r\n", "\n")
    if text and not text.endswith("\n"):
        issues.append(Issue(relpath, text.count("\n") + 1, "final-newline", "end the file with a newline"))
    lines = text.split("\n")
    if kind != "markdown":
        for number, line in enumerate(lines, start=1):
            if line != line.rstrip(" \t"):
                issues.append(Issue(relpath, number, "trailing-whitespace", "remove trailing whitespace"))

    skip, header_issues = check_header(kind, lines)
    issues.extend(Issue(relpath, line, "header", message) for line, message in header_issues)

    tokenize_error = None
    if kind == "cpp":
        hits = scan_cpp(text)
    elif kind == "webcode":
        hits = scan_webcode(text)
    elif kind == "svelte":
        hits = scan_svelte(text)
    elif kind == "html":
        hits = scan_markup_comments(text)
    elif kind == "python":
        hits, tokenize_error = scan_python(text)
    elif kind == "cmake":
        hits = scan_cmake(text)
    elif kind == "shell":
        hits = scan_shell(text)
    elif kind == "powershell":
        hits = scan_powershell(text)
    elif kind == "yaml":
        hits = scan_yaml(text)
    elif kind == "linehash":
        hits = scan_line_start(text, ("#",))
    elif kind in ("editorconfig", "conf"):
        hits = scan_line_start(text, ("#", ";"))
    elif kind == "sql":
        hits = scan_sql(text)
    elif kind == "batch":
        hits = scan_batch(text)
    else:
        hits = scan_markdown(text)
    if tokenize_error:
        issues.append(Issue(relpath, 1, "tokenize", f"could not tokenize the file: {tokenize_error}"))
    for line in sorted(set(hits)):
        if line > skip:
            issues.append(Issue(relpath, line, "comment", "comments are not allowed outside the branding header"))

    name_without_template = name[:-3] if name.endswith(".in") else name
    if kind == "cpp" and os.path.splitext(name_without_template)[1].lower() in HEADER_EXTENSIONS:
        issues.extend(Issue(relpath, line, "include-guard", message) for line, message in check_include_guard(relpath, lines, skip))

    return sorted(issues, key=lambda issue: (issue.line, issue.rule))


def is_excluded(relpath):
    first = relpath.split("/", 1)[0]
    return first in (".git", "deps") or first.startswith("build")


def collect_files(root):
    try:
        output = subprocess.run(
            ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
            cwd=root, capture_output=True, check=True).stdout
        candidates = [path for path in output.decode("utf-8").split("\0") if path]
    except (OSError, subprocess.CalledProcessError):
        candidates = []
        for directory, subdirectories, filenames in os.walk(root):
            relative_directory = os.path.relpath(directory, root).replace(os.sep, "/")
            subdirectories[:] = [d for d in subdirectories if not is_excluded(d if relative_directory == "." else f"{relative_directory}/{d}")]
            for filename in filenames:
                candidates.append(filename if relative_directory == "." else f"{relative_directory}/{filename}")
    return sorted(path for path in set(candidates) if not is_excluded(path) and os.path.isfile(os.path.join(root, path)))


def main(argv=None):
    default_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser = argparse.ArgumentParser(description="Project Ambrose codestyle checker")
    parser.add_argument("paths", nargs="*", help="files to check; defaults to every tracked and unignored file")
    parser.add_argument("--root", default=default_root, help="repository root")
    args = parser.parse_args(argv)
    root = os.path.abspath(args.root)
    if args.paths:
        relpaths = [os.path.relpath(os.path.abspath(path), root).replace(os.sep, "/") for path in args.paths]
    else:
        relpaths = collect_files(root)
    issues = []
    for relpath in relpaths:
        with open(os.path.join(root, relpath), "rb") as handle:
            issues.extend(check_file(relpath, handle.read()))
    for issue in issues:
        print(issue)
    print(f"codestyle: {len(relpaths)} files checked, {len(issues)} issue(s)")
    return 1 if issues else 0


if __name__ == "__main__":
    sys.exit(main())
