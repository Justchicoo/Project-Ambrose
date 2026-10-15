/*
 * Project Ambrose by Imjustchico
 * The /api/data/<kind>/ route family: paged, stamped, byte-free answers over each kind of decoded data an app holds, with budgets that name themselves when a request passes one.
 */

#ifndef AMBROSE_ADMINDATAVIEW_H
#define AMBROSE_ADMINDATAVIEW_H

#include <cstdint>
#include <functional>
#include <string>

class AdminRouter;
class TypeRegistry;

class AdminDataView
{
public:
    static constexpr int SchemaVersion = 1;
    static constexpr std::size_t MaxRowsPerPage = 200;

    AdminDataView() = delete;

    struct Stamp
    {
        std::string Revision;
        std::string ProgramSha256;
        uint64_t Generation = 0;
    };

    using StampSource = std::function<Stamp()>;

    static void Register(AdminRouter& router, StampSource source, TypeRegistry& registry);
    static void RegisterKind(std::string_view kind, std::function<uint64_t()> generation);
    static std::vector<std::pair<std::string, uint64_t>> DataKinds();
};

#endif
