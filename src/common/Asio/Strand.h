/*
 * Project Ambrose by Imjustchico
 * Strand type over the io_context executor, with helpers to create one and to post or dispatch through it.
 */

#ifndef AMBROSE_STRAND_H
#define AMBROSE_STRAND_H

#include "IoContext.h"

#include <asio/strand.hpp>

#include <utility>

namespace Ambrose::Asio
{
    using Strand = asio::strand<IoContext::Executor>;

    inline Strand MakeStrand(IoContext& context)
    {
        return asio::make_strand(context.GetExecutor());
    }

    template<typename Handler>
    void Post(Strand const& strand, Handler&& handler)
    {
        asio::post(strand, std::forward<Handler>(handler));
    }

    template<typename Handler>
    void Dispatch(Strand const& strand, Handler&& handler)
    {
        asio::dispatch(strand, std::forward<Handler>(handler));
    }
}

#endif
