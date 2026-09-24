/*
 * Project Ambrose by Imjustchico
 * Lock-free multi-producer single-consumer queue of values, linked through atomic node pointers.
 */

#ifndef AMBROSE_MPSCQUEUE_H
#define AMBROSE_MPSCQUEUE_H

#include <atomic>
#include <optional>
#include <utility>

template<typename T>
class MPSCQueue
{
public:
    MPSCQueue() : _head(new Node()), _tail(_head.load(std::memory_order_relaxed))
    {
    }

    ~MPSCQueue()
    {
        Node* node = _tail;
        while (node != nullptr)
        {
            Node* const next = node->Next.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
    }

    MPSCQueue(MPSCQueue const&) = delete;
    MPSCQueue& operator=(MPSCQueue const&) = delete;

    void Enqueue(T value)
    {
        Node* const node = new Node(std::move(value));
        Node* const previous = _head.exchange(node, std::memory_order_acq_rel);
        previous->Next.store(node, std::memory_order_release);
    }

    bool Dequeue(T& result)
    {
        Node* const next = _tail->Next.load(std::memory_order_acquire);
        if (next == nullptr)
            return false;
        result = std::move(*next->Value);
        next->Value.reset();
        delete _tail;
        _tail = next;
        return true;
    }

    bool Empty() const
    {
        return _tail->Next.load(std::memory_order_acquire) == nullptr;
    }

private:
    struct Node
    {
        Node() = default;

        explicit Node(T value) : Value(std::move(value))
        {
        }

        std::optional<T> Value;
        std::atomic<Node*> Next{ nullptr };
    };

    std::atomic<Node*> _head;
    Node* _tail;
};

#endif
