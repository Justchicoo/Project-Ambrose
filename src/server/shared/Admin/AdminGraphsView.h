/*
 * Project Ambrose by Imjustchico
 * The history behind the panel's graphs: GET /api/graphs names every subject and the series each one holds, and GET /api/graphs/range answers a range of one of them, naming the subject and the series in the query rather than in the path, so a name with a space or a slash in it needs no encoding of its own. The caller says how far back it wants and how many points it will draw, and the answer never carries more than that, because a graph of a month is drawn on a few hundred pixels and sending a month of raw samples would cost more to transfer than to draw. A stretch nothing was written into comes back as a point that is not present rather than as a zero, so the panel can leave a gap where an app was stopped instead of drawing a line along the floor. Both sit behind the same guard as the rest of the admin API.
 */

#ifndef AMBROSE_ADMINGRAPHSVIEW_H
#define AMBROSE_ADMINGRAPHSVIEW_H

#include "SeriesStore.h"

#include <functional>
#include <string>

class AdminRouter;

class AdminGraphsView
{
public:
    static constexpr int SchemaVersion = 1;
    static constexpr std::size_t DefaultPoints = 300;
    static constexpr std::size_t MostPoints = 2000;
    static constexpr int64 DefaultSpanMilliseconds = 60LL * 60 * 1000;

    AdminGraphsView() = delete;

    static std::string SubjectsJson(Ambrose::SeriesStore const& store);
    static std::string RangeJson(Ambrose::SeriesStore const& store, std::string const& subject, std::string const& series,
        int64 fromMilliseconds, int64 toMilliseconds, std::size_t mostPoints);

    static void Register(AdminRouter& router, std::function<Ambrose::SeriesStore const&()> source);
};

#endif
