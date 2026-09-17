/*
 * Project Ambrose by Imjustchico
 * A client configuration file held as the text the client wrote: it sets one key's value inside one table, empties a key wherever the file holds it, and renames the root element, each by splicing bytes, so the declaration, indentation, attribute spelling, line endings and every other byte survive unchanged, because the r806919 client reads its own settings back only from a file it still recognizes and falls back to its built-in defaults when a rewritten one replaces it; a table, record or key the file lacks is added in the file's own style, and an added table joins _TableList when the file lists its tables.
 */

#ifndef AMBROSE_CLIENTCONFIGTEXT_H
#define AMBROSE_CLIENTCONFIGTEXT_H

#include <string>
#include <string_view>

class ClientConfigText
{
public:
    explicit ClientConfigText(std::string text);

    std::string const& Text() const;
    void RenameRoot(std::string_view name);
    void SetKey(std::string_view table, std::string_view key, std::string_view type, std::string_view value);
    void EmptyKey(std::string_view key);

private:
    void InsertTable(std::string_view table, std::string_view step, std::string_view lineEnd);
    void ListTable(std::string_view table, std::string_view step, std::string_view lineEnd);

    std::string _text;
};

#endif
