/*
 * Project Ambrose by Imjustchico
 * Advances a mobile through ordered path nodes at its template speed and scale, holding at waits and turning or wrapping at the route ends.
 */

#ifndef AMBROSE_PATHMOVEMENTGENERATOR_H
#define AMBROSE_PATHMOVEMENTGENERATOR_H

#include "Types.h"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct PathMovementPosition
{
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;

    bool operator==(PathMovementPosition const&) const = default;
};

struct PathMovementNode
{
    uint32 Id = 0;
    PathMovementPosition Position;
    std::chrono::milliseconds Wait{ 0 };
};

enum class PathTraversalMode : uint8
{
    Loop,
    PingPong
};

enum class PathInitialDirection : int8
{
    Reverse = -1,
    Forward = 1
};

struct PathMovementStep
{
    PathMovementPosition Position;
    uint32 CurrentNodeId = 0;
    uint32 NextNodeId = 0;
    bool Moving = false;
    bool Changed = false;
};

class PathMovementGenerator
{
public:
    static constexpr std::size_t MaxTransitionsPerUpdate = 4096;

    static std::optional<PathMovementGenerator> Create(std::vector<PathMovementNode> nodes, std::size_t startNode, float movementSpeed,
        float movementScale, PathTraversalMode traversal, PathInitialDirection direction, std::string& error);

    std::optional<PathMovementStep> Advance(std::chrono::duration<double> elapsed, std::string& error);

    PathMovementPosition const& GetPosition() const noexcept { return _position; }
    uint32 GetCurrentNodeId() const noexcept { return _nodes[_currentNode].Id; }
    uint32 GetNextNodeId() const noexcept { return _nodes[_nextNode].Id; }
    float GetSpeed() const noexcept { return _speed; }

private:
    PathMovementGenerator(std::vector<PathMovementNode> nodes, std::size_t startNode, float speed, PathTraversalMode traversal,
        PathInitialDirection direction);

    std::size_t NextNode(std::size_t current, int direction) const noexcept;
    void Reach(std::size_t node) noexcept;
    PathMovementStep MakeStep(PathMovementPosition const& before) const noexcept;

    std::vector<PathMovementNode> _nodes;
    PathMovementPosition _position;
    std::size_t _currentNode;
    std::size_t _nextNode;
    int _direction;
    float _speed;
    PathTraversalMode _traversal;
    double _cycleSeconds;
    double _waitRemaining = 0.0;
};

#endif
