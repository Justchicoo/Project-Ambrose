/*
 * Project Ambrose by Imjustchico
 * Reads what each app counts about itself and writes it into the same history the resource readings go into, because how many players a realm holds and how long its tick took are the numbers an operator watches beside processor and memory, and only the app knows them. A gauge is written as it stands, since it already describes a moment. A histogram that has had nothing observed in a stretch is written nowhere rather than as a mean of no observations, and a counter is never mistaken for one, because what separates them is what the app called them and not whether their count happens to be zero. A counter and a histogram are totals since the app started, which say nothing on their own, so what is written is the difference from the last reading over the time between the two: a rate for a counter and the mean of the observations made in that stretch for a histogram, which is the tick time of those seconds rather than of every second since the server came up. An app that has just started has no previous reading, so its first round writes only its gauges. The reading is taken through a function, so a test hands it whatever an app might answer and the supervisor hands it the real relay.
 */

#ifndef AMBROSE_PUBLISHEDSAMPLER_H
#define AMBROSE_PUBLISHEDSAMPLER_H

#include "ManagedApp.h"
#include "SeriesStore.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

class PublishedSampler
{
public:
    using Reader = std::function<std::optional<std::string>(std::string const& app)>;

    struct Total
    {
        bool Histogram = false;
        double Sum = 0.0;
        double Count = 0.0;
    };

    using Totals = std::map<std::string, Total>;

    PublishedSampler(Ambrose::SeriesStore& store, Reader reader);

    std::vector<std::string> Sample(std::vector<AppSnapshot> const& apps, int64 atMilliseconds);
    void Forget(std::string const& app);

    static std::map<std::string, double> Read(std::string const& json, Totals& totals);

private:
    struct Last
    {
        int64 AtMilliseconds = 0;
        Totals Amounts;
        bool Seen = false;
    };

    Ambrose::SeriesStore& _store;
    Reader _reader;
    std::map<std::string, Last> _last;
};

#endif
