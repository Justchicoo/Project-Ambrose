/*
 * Project Ambrose by Imjustchico
 * The cells of one zone instance, square on the ground plane, each holding the ids placed in it, so the objects and wizards near a point are found by looking at the few cells a radius reaches rather than at everything in the instance; a place moves an id to its new cell, a query measures the whole distance, height included, and gives ids in ascending order, and a changed cell size files every id again.
 */

#ifndef AMBROSE_GRID_H
#define AMBROSE_GRID_H

#include "Types.h"

#include <cstddef>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

struct GridPoint
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
};

struct GridNeighbor
{
    uint64 Id = 0;
    float DistanceSquared = 0.0f;
};

class Grid
{
public:
    static constexpr float MinimumCellSize = 1.0f;

    explicit Grid(float cellSize);

    float GetCellSize() const noexcept { return _cellSize; }
    void SetCellSize(float cellSize);

    void Place(uint64 id, GridPoint const& at);
    bool Remove(uint64 id);
    bool Contains(uint64 id) const;
    GridPoint const* Find(uint64 id) const;
    std::size_t Size() const noexcept { return _placed.size(); }

    std::vector<GridNeighbor> Near(GridPoint const& at, float radius) const;

private:
    using Cell = std::pair<int64, int64>;

    struct Placed
    {
        GridPoint At;
        Cell In;
    };

    Cell CellOf(GridPoint const& at) const noexcept;
    void Unfile(uint64 id, Cell const& cell);

    float _cellSize;
    std::unordered_map<uint64, Placed> _placed;
    std::map<Cell, std::vector<uint64>> _cells;
};

#endif
