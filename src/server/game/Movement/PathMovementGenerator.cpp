/*
 * Project Ambrose by Imjustchico
 * Validates a path movement route and advances a mobile across every node reached in a tick, carrying unused tick time into the next leg and folding whole route cycles out of a long tick.
 */

#include "PathMovementGenerator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
    bool IsFinite(PathMovementPosition const& position) noexcept
    {
        return std::isfinite(position.X) && std::isfinite(position.Y) && std::isfinite(position.Z);
    }

    double Distance(PathMovementPosition const& from, PathMovementPosition const& to) noexcept
    {
        double const dx = to.X - from.X;
        double const dy = to.Y - from.Y;
        double const dz = to.Z - from.Z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    double WaitSeconds(PathMovementNode const& node) noexcept
    {
        return std::chrono::duration<double>(node.Wait).count();
    }

    double CycleSeconds(std::vector<PathMovementNode> const& nodes, double speed, PathTraversalMode traversal) noexcept
    {
        if (speed <= 0.0)
            return 0.0;
        std::size_t const count = nodes.size();
        double cycle = 0.0;
        for (std::size_t index = 0; index + 1 < count; ++index)
            cycle += Distance(nodes[index].Position, nodes[index + 1].Position) / speed;
        if (traversal == PathTraversalMode::Loop)
        {
            cycle += Distance(nodes[count - 1].Position, nodes[0].Position) / speed;
            for (PathMovementNode const& node : nodes)
                cycle += WaitSeconds(node);
            return cycle;
        }
        cycle *= 2.0;
        cycle += WaitSeconds(nodes[0]) + WaitSeconds(nodes[count - 1]);
        for (std::size_t index = 1; index + 1 < count; ++index)
            cycle += 2.0 * WaitSeconds(nodes[index]);
        return cycle;
    }
}

PathMovementGenerator::PathMovementGenerator(std::vector<PathMovementNode> nodes, std::size_t startNode, float speed, PathTraversalMode traversal,
    PathInitialDirection direction)
    : _nodes(std::move(nodes)), _position(_nodes[startNode].Position), _currentNode(startNode),
      _direction(static_cast<int>(direction)), _speed(speed), _traversal(traversal), _cycleSeconds(CycleSeconds(_nodes, speed, traversal))
{
    _nextNode = NextNode(_currentNode, _direction);
    if (_traversal == PathTraversalMode::PingPong &&
        ((_currentNode == 0 && _direction < 0) || (_currentNode + 1 == _nodes.size() && _direction > 0)))
    {
        _direction = -_direction;
        _nextNode = NextNode(_currentNode, _direction);
    }
}

std::optional<PathMovementGenerator> PathMovementGenerator::Create(std::vector<PathMovementNode> nodes, std::size_t startNode, float movementSpeed,
    float movementScale, PathTraversalMode traversal, PathInitialDirection direction, std::string& error)
{
    if (nodes.size() < 2)
    {
        error = "a path needs at least two nodes";
        return std::nullopt;
    }
    if (startNode >= nodes.size())
    {
        error = "the starting node is outside the path";
        return std::nullopt;
    }
    if (!std::isfinite(movementSpeed) || movementSpeed < 0.0f || !std::isfinite(movementScale) || movementScale < 0.0f)
    {
        error = "movement speed and scale must be finite and nonnegative";
        return std::nullopt;
    }
    float const speed = movementSpeed * movementScale;
    if (!std::isfinite(speed))
    {
        error = "movement speed multiplied by scale is not finite";
        return std::nullopt;
    }
    if (traversal != PathTraversalMode::Loop && traversal != PathTraversalMode::PingPong)
    {
        error = "the path traversal mode is not supported";
        return std::nullopt;
    }
    if (direction != PathInitialDirection::Forward && direction != PathInitialDirection::Reverse)
    {
        error = "the initial path direction is not supported";
        return std::nullopt;
    }
    for (std::size_t index = 0; index < nodes.size(); ++index)
    {
        if (!IsFinite(nodes[index].Position))
        {
            error = "path node " + std::to_string(nodes[index].Id) + " has a non-finite position";
            return std::nullopt;
        }
        if (nodes[index].Wait < std::chrono::milliseconds::zero())
        {
            error = "path node " + std::to_string(nodes[index].Id) + " has a negative wait";
            return std::nullopt;
        }
    }

    if (speed > 0.0f && !(CycleSeconds(nodes, speed, traversal) > 0.0))
    {
        error = "a moving path needs nodes at more than one position or a wait at some node";
        return std::nullopt;
    }

    error.clear();
    return PathMovementGenerator(std::move(nodes), startNode, speed, traversal, direction);
}

std::size_t PathMovementGenerator::NextNode(std::size_t current, int direction) const noexcept
{
    if (direction > 0)
    {
        if (current + 1 < _nodes.size())
            return current + 1;
        return _traversal == PathTraversalMode::Loop ? 0 : current - 1;
    }
    if (current > 0)
        return current - 1;
    return _traversal == PathTraversalMode::Loop ? _nodes.size() - 1 : current + 1;
}

void PathMovementGenerator::Reach(std::size_t node) noexcept
{
    _currentNode = node;
    if (_traversal == PathTraversalMode::PingPong && ((_currentNode == 0 && _direction < 0) || (_currentNode + 1 == _nodes.size() && _direction > 0)))
        _direction = -_direction;
    _nextNode = NextNode(_currentNode, _direction);
    _waitRemaining = WaitSeconds(_nodes[_currentNode]);
}

PathMovementStep PathMovementGenerator::MakeStep(PathMovementPosition const& before) const noexcept
{
    return { _position, _nodes[_currentNode].Id, _nodes[_nextNode].Id, _speed > 0.0f && _waitRemaining == 0.0,
        _position != before };
}

std::optional<PathMovementStep> PathMovementGenerator::Advance(std::chrono::duration<double> elapsed, std::string& error)
{
    double remaining = elapsed.count();
    if (!std::isfinite(remaining) || remaining < 0.0)
    {
        error = "elapsed path time must be finite and nonnegative";
        return std::nullopt;
    }

    if (_cycleSeconds > 0.0 && remaining > _cycleSeconds)
        remaining = std::fmod(remaining, _cycleSeconds);

    PathMovementPosition const before = _position;
    std::size_t transitions = 0;
    while (remaining > 0.0)
    {
        if (_waitRemaining > 0.0)
        {
            double const waited = std::min(remaining, _waitRemaining);
            remaining -= waited;
            _waitRemaining -= waited;
            if (_waitRemaining > 0.0)
                break;
        }
        if (_speed == 0.0f)
            break;

        PathMovementPosition const& target = _nodes[_nextNode].Position;
        double const dx = static_cast<double>(target.X) - _position.X;
        double const dy = static_cast<double>(target.Y) - _position.Y;
        double const dz = static_cast<double>(target.Z) - _position.Z;
        double const distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance == 0.0)
        {
            Reach(_nextNode);
            if (++transitions > MaxTransitionsPerUpdate)
            {
                error = "the path exceeds the per-update node transition limit";
                return std::nullopt;
            }
            continue;
        }

        double const travelTime = distance / _speed;
        double const timeTolerance = std::numeric_limits<double>::epsilon() *
            std::max({ 1.0, remaining, travelTime }) * 256.0;
        if (remaining + timeTolerance < travelTime)
        {
            double const fraction = remaining / travelTime;
            _position.X += dx * fraction;
            _position.Y += dy * fraction;
            _position.Z += dz * fraction;
            remaining = 0.0;
            break;
        }

        _position = target;
        remaining -= travelTime;
        if (remaining <= timeTolerance)
            remaining = 0.0;
        Reach(_nextNode);
        if (++transitions > MaxTransitionsPerUpdate)
        {
            error = "the path exceeds the per-update node transition limit";
            return std::nullopt;
        }
    }

    error.clear();
    return MakeStep(before);
}
