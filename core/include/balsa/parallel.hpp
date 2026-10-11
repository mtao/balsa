#if !defined(BALSA_PARALLEL_HPP)
#define BALSA_PARALLEL_HPP

/// @file parallel.hpp
/// Parallel execution utilities — thin abstraction over C++17 parallel
/// algorithms backed by TBB.
///
/// When `BALSA_PARALLEL` is defined (via `-Dparallel=true`), the helpers
/// in `balsa::parallel` dispatch to `std::execution::par` overloads.
/// Otherwise they fall back to plain serial execution — zero overhead.
///
/// Usage:
///   #include "balsa/parallel.hpp"
///
///   balsa::parallel::for_each(0, n, [&](size_t i) { ... });

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <numeric>

#ifdef BALSA_PARALLEL

#include <execution>

#endif // BALSA_PARALLEL

namespace balsa::parallel {

// ── Execution policy tags ───────────────────────────────────────────────────
//
// Lightweight tag types for selecting serial vs. parallel execution in bulk
// operations (StoredAttribute::assign, set_from, etc.).
//
// These mirror the std::execution policy tags but are independent of
// <execution> so they compile even when BALSA_PARALLEL is not defined.

/// Tag type requesting sequential execution.
struct seq_t {
    explicit seq_t() = default;
};

/// Tag type requesting parallel execution (falls back to serial when
/// BALSA_PARALLEL is not defined).
struct par_t {
    explicit par_t() = default;
};

/// Instance: force sequential execution.
inline constexpr seq_t seq{};

/// Instance: request parallel execution (default for bulk operations).
inline constexpr par_t par{};

/// Concept satisfied by seq_t or par_t.
template <typename P>
concept execution_policy = std::same_as<P, seq_t> || std::same_as<P, par_t>;

namespace detail {

    /// Minimal random-access counting iterator for `parallel::for_each`.
    ///
    /// Satisfies `std::random_access_iterator` so that libstdc++'s
    /// `std::for_each(execution::par, ...)` can partition the range
    /// efficiently.  Avoids the heap allocation that a
    /// `std::vector<size_t>` + `std::iota` approach requires.
    class CountingIterator {
      public:
        // ── Iterator traits ─────────────────────────────────────────────
        using iterator_category = std::random_access_iterator_tag;
        using value_type = size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = const size_t *;
        using reference = size_t;

        constexpr CountingIterator() noexcept = default;
        constexpr explicit CountingIterator(size_t v) noexcept : m_val(v) {}

        constexpr auto operator*() const noexcept -> size_t { return m_val; }
        constexpr auto operator[](difference_type n) const noexcept -> size_t {
            return m_val + static_cast<size_t>(n);
        }

        constexpr auto operator++() noexcept -> CountingIterator & {
            ++m_val;
            return *this;
        }
        constexpr auto operator++(int) noexcept -> CountingIterator {
            auto tmp = *this;
            ++m_val;
            return tmp;
        }
        constexpr auto operator--() noexcept -> CountingIterator & {
            --m_val;
            return *this;
        }
        constexpr auto operator--(int) noexcept -> CountingIterator {
            auto tmp = *this;
            --m_val;
            return tmp;
        }

        constexpr auto operator+=(difference_type n) noexcept
            -> CountingIterator & {
            m_val += static_cast<size_t>(n);
            return *this;
        }
        constexpr auto operator-=(difference_type n) noexcept
            -> CountingIterator & {
            m_val -= static_cast<size_t>(n);
            return *this;
        }

        friend constexpr auto operator+(CountingIterator it,
                                        difference_type n) noexcept
            -> CountingIterator {
            return CountingIterator{it.m_val + static_cast<size_t>(n)};
        }
        friend constexpr auto operator+(difference_type n,
                                        CountingIterator it) noexcept
            -> CountingIterator {
            return CountingIterator{it.m_val + static_cast<size_t>(n)};
        }
        friend constexpr auto operator-(CountingIterator it,
                                        difference_type n) noexcept
            -> CountingIterator {
            return CountingIterator{it.m_val - static_cast<size_t>(n)};
        }
        friend constexpr auto operator-(CountingIterator a,
                                        CountingIterator b) noexcept
            -> difference_type {
            return static_cast<difference_type>(a.m_val)
                   - static_cast<difference_type>(b.m_val);
        }

        friend constexpr auto operator==(CountingIterator a,
                                         CountingIterator b) noexcept -> bool {
            return a.m_val == b.m_val;
        }
        friend constexpr auto operator<=>(CountingIterator a,
                                          CountingIterator b) noexcept {
            return a.m_val <=> b.m_val;
        }

      private:
        size_t m_val = 0;
    };

} // namespace detail

/// Apply @p fn to each integer in [begin, end), potentially in parallel.
///
/// When BALSA_PARALLEL is enabled, uses `std::for_each` with
/// `std::execution::par`.  Otherwise, a plain sequential loop.
///
/// Uses a zero-allocation counting iterator to avoid heap overhead.
///
/// @param begin  First index (inclusive).
/// @param end    Past-the-end index.
/// @param fn     Callable `(size_t) -> void`.
template <typename Fn>
void for_each(size_t begin, size_t end, Fn &&fn) {
    if (begin >= end) return;

#ifdef BALSA_PARALLEL

    std::for_each(std::execution::par,
                  detail::CountingIterator{begin},
                  detail::CountingIterator{end},
                  std::forward<Fn>(fn));

#else

    for (size_t i = begin; i < end; ++i) { fn(i); }

#endif
}

/// Apply @p fn to each element in [first, last), potentially in parallel.
///
/// @param first  Iterator to the first element.
/// @param last   Iterator past the last element.
/// @param fn     Callable taking a reference to the element.
template <typename It, typename Fn>
void for_each_iter(It first, It last, Fn &&fn) {
#ifdef BALSA_PARALLEL
    std::for_each(std::execution::par, first, last, std::forward<Fn>(fn));
#else
    std::for_each(first, last, std::forward<Fn>(fn));
#endif
}

/// Sort [first, last) using @p comp, potentially in parallel.
///
/// When BALSA_PARALLEL is enabled, uses `std::sort` with
/// `std::execution::par`.  Otherwise, plain `std::sort`.
///
/// @param first  Iterator to the first element.
/// @param last   Iterator past the last element.
/// @param comp   Comparison functor.
template <typename It, typename Comp>
void sort(It first, It last, Comp &&comp) {
#ifdef BALSA_PARALLEL
    std::sort(std::execution::par, first, last, std::forward<Comp>(comp));
#else
    std::sort(first, last, std::forward<Comp>(comp));
#endif
}

/// Parallel prefix-sum (exclusive scan) of @p count elements, potentially
/// in parallel.
///
/// Writes to @p out[0..count) where `out[0] = init`,
/// `out[i] = init + in[0] + ... + in[i-1]`.
///
/// @param in     Input iterator.
/// @param count  Number of elements.
/// @param out    Output iterator (may alias @p in if T allows in-place).
/// @param init   Initial value.
template <typename InIt, typename OutIt, typename T>
void exclusive_scan(InIt in, size_t count, OutIt out, T init) {
#ifdef BALSA_PARALLEL
    std::exclusive_scan(std::execution::par, in, in + count, out, init);
#else
    std::exclusive_scan(in, in + count, out, init);
#endif
}

} // namespace balsa::parallel
#endif
