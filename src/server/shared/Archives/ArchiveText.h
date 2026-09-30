/*
 * Project Ambrose by Imjustchico
 * What an archive's text files say about the client's classes: every property name, m_ and a C++ identifier, and every class name that a plain XML, text or leftover .notxml entry spells out, which the client's own data writes wherever it keeps an object as text rather than BINd, each a candidate its hash must still prove.
 */

#ifndef AMBROSE_ARCHIVETEXT_H
#define AMBROSE_ARCHIVETEXT_H

#include <string>
#include <vector>

class KiwadArchive;

namespace ArchiveText
{
    std::vector<std::string> PropertyNames(KiwadArchive const& archive);
    std::vector<std::string> ClassNames(KiwadArchive const& archive);
}

#endif
