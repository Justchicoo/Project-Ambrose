/*
 * Project Ambrose by Imjustchico
 * Answers the graphs. A range is read from the query rather than the path so a caller can change what it is looking at without the route changing, and every number it sends is clamped here rather than trusted, because a caller asking for a hundred thousand points would otherwise decide how much work this server does. A point that is not present is written as a null value rather than left out, so the panel draws a gap at the right moment on the time axis instead of joining the two samples either side of a stopped app.
 */

#include "AdminGraphsView.h"
#include "AdminRouter.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <utility>

namespace
{
    int64 NowMilliseconds()
    {
        return static_cast<int64>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    std::optional<int64> Whole(std::string_view text)
    {
        int64 value = 0;
        auto const [stop, code] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (code != std::errc() || stop != text.data() + text.size())
            return std::nullopt;
        return value;
    }
}

std::string AdminGraphsView::SubjectsJson(Ambrose::SeriesStore const& store)
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["now_epoch_ms"] = NowMilliseconds();
    nlohmann::json subjects = nlohmann::json::array();
    for (std::string const& subject : store.Subjects())
    {
        nlohmann::json entry;
        entry["subject"] = subject;
        entry["series"] = store.SeriesOf(subject);
        subjects.push_back(std::move(entry));
    }
    body["subjects"] = std::move(subjects);
    return body.dump();
}

std::string AdminGraphsView::RangeJson(Ambrose::SeriesStore const& store, std::string const& subject, std::string const& series,
    int64 fromMilliseconds, int64 toMilliseconds, std::size_t mostPoints)
{
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["subject"] = subject;
    body["series"] = series;
    body["from_epoch_ms"] = fromMilliseconds;
    body["to_epoch_ms"] = toMilliseconds;

    std::vector<Ambrose::SeriesPoint> const points = store.Between(subject, series, fromMilliseconds, toMilliseconds, mostPoints);
    nlohmann::json at = nlohmann::json::array();
    nlohmann::json values = nlohmann::json::array();
    nlohmann::json lowest = nlohmann::json::array();
    nlohmann::json highest = nlohmann::json::array();
    std::size_t present = 0;
    for (Ambrose::SeriesPoint const& point : points)
    {
        at.push_back(point.AtMilliseconds);
        if (point.Present)
        {
            ++present;
            values.push_back(point.Mean);
            lowest.push_back(point.Lowest);
            highest.push_back(point.Highest);
        }
        else
        {
            values.push_back(nullptr);
            lowest.push_back(nullptr);
            highest.push_back(nullptr);
        }
    }
    body["at_epoch_ms"] = std::move(at);
    body["values"] = std::move(values);
    body["lowest"] = std::move(lowest);
    body["highest"] = std::move(highest);
    body["points"] = points.size();
    body["present"] = present;
    return body.dump();
}

void AdminGraphsView::Register(AdminRouter& router, std::function<Ambrose::SeriesStore const&()> source)
{
    router.AddGuarded("GET", "/api/graphs", "status.read", [source](AdminRequest const&)
    {
        return AdminResponse::Json(200, SubjectsJson(source()));
    });

    router.AddGuarded("GET", "/api/graphs/range", "status.read", [source](AdminRequest const& request)
    {
        std::string const subject(request.Query("subject"));
        std::string const series(request.Query("series"));
        if (subject.empty() || series.empty())
            return AdminResponse::Invalid("A range names a subject and a series",
                { { "subject", "Name the app or realm the history belongs to" }, { "series", "Name the series within it" } });

        Ambrose::SeriesStore const& store = source();
        if (!store.Has(subject, series))
            return AdminResponse::Problem(404, "not_found", "Nothing has been recorded for " + subject + " " + series);

        int64 const now = NowMilliseconds();
        int64 to = now;
        if (std::optional<int64> const asked = Whole(request.Query("to")))
            to = *asked;
        int64 from = to - DefaultSpanMilliseconds;
        if (std::optional<int64> const asked = Whole(request.Query("from")))
            from = *asked;
        if (from > to)
            std::swap(from, to);

        std::size_t points = DefaultPoints;
        if (std::optional<int64> const asked = Whole(request.Query("points")))
            points = static_cast<std::size_t>(std::clamp<int64>(*asked, 1, static_cast<int64>(MostPoints)));

        return AdminResponse::Json(200, RangeJson(store, subject, series, from, to, points));
    });
}
