/*
 * Project Ambrose by Imjustchico
 * Files each id in the cell its ground position falls in, found by flooring the position over the cell size, and answers a query from the square of cells the radius reaches, keeping only ids whose whole distance is within it; a radius of zero or less, or one no finite cell count covers, reaches every id placed.
 */

#include "Grid.h"

#include <algorithm>
#include <cmath>
#include <limits>

Grid::Grid(float cellSize) : _cellSize(std::max(cellSize, MinimumCellSize))
{
}

Grid::Cell Grid::CellOf(GridPoint const& at) const noexcept
{
    auto const index = [this](float coordinate)
    {
        double const cell = std::floor(static_cast<double>(coordinate) / _cellSize);
        double const limit = static_cast<double>(std::numeric_limits<int64>::max() / 2);
        return static_cast<int64>(std::clamp(std::isfinite(cell) ? cell : 0.0, -limit, limit));
    };
    return { index(at.X), index(at.Y) };
}

void Grid::Unfile(uint64 id, Cell const& cell)
{
    auto const found = _cells.find(cell);
    if (found == _cells.end())
        return;
    std::vector<uint64>& ids = found->second;
    ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
    if (ids.empty())
        _cells.erase(found);
}

void Grid::SetCellSize(float cellSize)
{
    float const size = std::max(cellSize, MinimumCellSize);
    if (size == _cellSize)
        return;
    _cellSize = size;
    _cells.clear();
    for (auto& [id, placed] : _placed)
    {
        placed.In = CellOf(placed.At);
        _cells[placed.In].push_back(id);
    }
}

void Grid::Place(uint64 id, GridPoint const& at)
{
    Cell const cell = CellOf(at);
    auto const found = _placed.find(id);
    if (found == _placed.end())
    {
        _placed.emplace(id, Placed{ at, cell });
        _cells[cell].push_back(id);
        return;
    }
    found->second.At = at;
    if (found->second.In == cell)
        return;
    Unfile(id, found->second.In);
    found->second.In = cell;
    _cells[cell].push_back(id);
}

bool Grid::Remove(uint64 id)
{
    auto const found = _placed.find(id);
    if (found == _placed.end())
        return false;
    Unfile(id, found->second.In);
    _placed.erase(found);
    return true;
}

bool Grid::Contains(uint64 id) const
{
    return _placed.contains(id);
}

GridPoint const* Grid::Find(uint64 id) const
{
    auto const found = _placed.find(id);
    return found == _placed.end() ? nullptr : &found->second.At;
}

std::vector<GridNeighbor> Grid::Near(GridPoint const& at, float radius) const
{
    std::vector<GridNeighbor> near;
    auto const measure = [&at](GridPoint const& other)
    {
        float const dx = other.X - at.X;
        float const dy = other.Y - at.Y;
        float const dz = other.Z - at.Z;
        return dx * dx + dy * dy + dz * dz;
    };
    double const reach = std::ceil(static_cast<double>(radius) / _cellSize);
    bool const everything = !(radius > 0.0f) || !std::isfinite(reach) || reach * reach > static_cast<double>(_cells.size()) * 4.0 + 64.0;
    if (everything)
    {
        for (auto const& [id, placed] : _placed)
        {
            float const distance = measure(placed.At);
            if (!(radius > 0.0f) || distance <= radius * radius)
                near.push_back({ id, distance });
        }
    }
    else
    {
        int64 const cells = static_cast<int64>(reach);
        Cell const center = CellOf(at);
        for (int64 x = center.first - cells; x <= center.first + cells; ++x)
        {
            for (int64 y = center.second - cells; y <= center.second + cells; ++y)
            {
                auto const found = _cells.find({ x, y });
                if (found == _cells.end())
                    continue;
                for (uint64 const id : found->second)
                {
                    float const distance = measure(_placed.at(id).At);
                    if (distance <= radius * radius)
                        near.push_back({ id, distance });
                }
            }
        }
    }
    std::sort(near.begin(), near.end(), [](GridNeighbor const& left, GridNeighbor const& right) { return left.Id < right.Id; });
    return near;
}
