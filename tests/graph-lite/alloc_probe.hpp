// The allocator override behind the alloc-failure sweeps: a countdown scope.
// The first threshold-1 allocations inside the scope go through; every
// allocation after that throws bad_alloc until the scope ends. Defining the
// global operator new/delete here keeps the override to the single test binary
// that includes it — the same pattern model_properties_test.cpp uses for its
// own operator new. Kept in a header, not the test file, so the sweep file
// stays inside the 500-line guard.
#ifndef GRAPH_LITE_TESTS_ALLOC_PROBE_HPP
#define GRAPH_LITE_TESTS_ALLOC_PROBE_HPP

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace graph_lite_test {

class AllocProbe {
public:
    class Scope {
    public:
        explicit Scope(int refuse_at) {
            countdown().store(refuse_at, std::memory_order_relaxed);
            active().store(true, std::memory_order_relaxed);
        }

        ~Scope() { active().store(false, std::memory_order_relaxed); }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    };

    static bool refuse_next() {
        if (!active().load(std::memory_order_relaxed))
            return false;
        return countdown().fetch_sub(1, std::memory_order_relaxed) <= 1;
    }

private:
    AllocProbe() = default;

    static std::atomic<bool>& active() {
        static std::atomic<bool> state{false};
        return state;
    }

    static std::atomic<int>& countdown() {
        static std::atomic<int> value{0};
        return value;
    }
};

} // namespace graph_lite_test

void* operator new(std::size_t size) {
    if (graph_lite_test::AllocProbe::refuse_next())
        throw std::bad_alloc();
    if (void* p = std::malloc(size == 0 ? 1 : size))
        return p;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    return ::operator new(size == 0 ? 1 : size);
}

// The nothrow forms route through the throwing ones so every allocation in the
// binary — gtest's temporary buffers included — passes the same probe and lands
// in the same malloc/free pair; a partial override pairs our deletes with
// someone else's news, which ASan reports as alloc-dealloc-mismatch.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size == 0 ? 1 : size);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](size == 0 ? 1 : size);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}

void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}

// Placement deallocation counterparts of the nothrow forms; the compiler calls
// them to clean up a failed constructor after a nothrow new.
void operator delete(void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}

#endif // GRAPH_LITE_TESTS_ALLOC_PROBE_HPP
