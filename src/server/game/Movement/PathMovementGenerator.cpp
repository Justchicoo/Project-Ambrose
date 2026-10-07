/*
 * Project Ambrose by Imjustchico
 * Validates a path movement route and advances a mobile across every node reached in a tick, carrying unused tick time into the next leg.
 */

#include "PathMovementGenerator.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    bool IsFinite(PathMovementPosition const& position) noexcept
    {
        return std::isfinite(position.X) && std::isfinite(position.Y) && std::isfinite(position.Z);
    }
}

PathMovementGenerator::PathMovementGenerator(std::vector<PathMovementNode> nodes, std::size_t startNode, float speed, PathTraversalMode traversal,
    PathInitialDirection direction)
    : _nodes(std::move(nodes)), _position(_nodes[startNode].Position), _currentNode(startNode),
      _direction(static_cast<int>(direction)), _speed(speed), _traversal(traversal)
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
    _waitRemaining = std::chrono::duration<double>(_nodes[_currentNode].Wait).count();
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
        if (remaining < travelTime)
        {
            double const fraction = remaining / travelTime;
            _position.X = static_cast<float>(static_cast<double>(_position.X) + dx * fraction);
            _position.Y = static_cast<float>(static_cast<double>(_position.Y) + dy * fraction);
            _position.Z = static_cast<float>(static_cast<double>(_position.Z) + dz * fraction);
            remaining = 0.0;
            break;
        }

        _position = target;
        remaining -= travelTime;
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
