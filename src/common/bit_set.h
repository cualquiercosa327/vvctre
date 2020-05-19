// This file is under the public domain.

#pragma once

#include <cstddef>
#ifdef _WIN32
#include <intrin.h>
#endif
#include <initializer_list>
#include <new>
#include <type_traits>
#include "common/common_types.h"

// Helper functions:

#ifdef _MSC_VER
template <typename T>
static inline int CountSetBits(T v) {
    // from https://graphics.stanford.edu/~seander/bithacks.html
    // GCC has this built in, but MSVC's intrinsic will only emit the actual
    // POPCNT instruction, which we're not depending on
    v = v - ((v >> 1) & (T) ~(T)0 / 3);
    v = (v & (T) ~(T)0 / 15 * 3) + ((v >> 2) & (T) ~(T)0 / 15 * 3);
    v = (v + (v >> 4)) & (T) ~(T)0 / 255 * 15;
    return (T)(v * ((T) ~(T)0 / 255)) >> (sizeof(T) - 1) * 8;
}
static inline int LeastSignificantSetBit(u8 val) {
    unsigned long index;
    _BitScanForward(&index, val);
    return (int)index;
}
static inline int LeastSignificantSetBit(u16 val) {
    unsigned long index;
    _BitScanForward(&index, val);
    return (int)index;
}
static inline int LeastSignificantSetBit(u32 val) {
    unsigned long index;
    _BitScanForward(&index, val);
    return (int)index;
}
static inline int LeastSignificantSetBit(u64 val) {
    unsigned long index;
    _BitScanForward64(&index, val);
    return (int)index;
}
#else
static inline int CountSetBits(u8 val) {
    return __builtin_popcount(val);
}
static inline int CountSetBits(u16 val) {
    return __builtin_popcount(val);
}
static inline int CountSetBits(u32 val) {
    return __builtin_popcount(val);
}
static inline int CountSetBits(u64 val) {
    return __builtin_popcountll(val);
}
static inline int LeastSignificantSetBit(u8 val) {
    return __builtin_ctz(val);
}
static inline int LeastSignificantSetBit(u16 val) {
    return __builtin_ctz(val);
}
static inline int LeastSignificantSetBit(u32 val) {
    return __builtin_ctz(val);
}
static inline int LeastSignificantSetBit(u64 val) {
    return __builtin_ctzll(val);
}
#endif

// Similar to std::bitset, this is a class which encapsulates a bitset, i.e.
// using the set bits of an integer to represent a set of integers.  Like that
// class, it acts like an array of bools:
//     BitSet<u32> bs;
//     bs[1] = true;
// but also like the underlying integer ([0] = least significant bit):
//     BitSet<u32> bs2 = ...;
//     bs = (bs ^ bs2) & BitSet<u32>(0xffff);
// The following additional functionality is provided:
// - Construction using an initializer list.
//     BitSet bs { 1, 2, 4, 8 };
// - Efficiently iterating through the set bits:
//     for (int i : bs)
//         [i is the *index* of a set bit]
//   (This uses the appropriate CPU instruction to find the next set bit in one
//   operation.)
// - Counting set bits using .Count() - see comment on that method.

// TODO: use constexpr when MSVC gets out of the Dark Ages

template <typename IntTy>
class BitSet {
    static_assert(!std::is_signed_v<IntTy>, "BitSet should not be used with signed types");

public:
    // A reference to a particular bit, returned from operator[].
    class Ref {
    public:
        Ref(Ref&& other) : m_bs(other.m_bs), m_mask(other.m_mask) {}
        Ref(BitSet* bs, IntTy mask) : m_bs(bs), m_mask(mask) {}
        operator bool() const {
            return (m_bs->m_val & m_mask) != 0;
        }
        bool operator=(bool set) {
            m_bs->m_val = (m_bs->m_val & ~m_mask) | (set ? m_mask : 0);
            return set;
        }

    private:
        BitSet* m_bs;
        IntTy m_mask;
    };

    // A STL-like iterator is required to be able to use range-based for loops.
    class Iterator {
    public:
        Iterator(const Iterator& other) : m_val(other.m_val) {}
        Iterator(IntTy val) : m_val(val) {}
        int operator*() {
            // This will never be called when m_val == 0, because that would be the end() iterator
            return LeastSignificantSetBit(m_val);
        }
        Iterator& operator++() {
            // Unset least significant set bit
            m_val &= m_val - IntTy(1);
            return *this;
        }
        Iterator operator++(int _) {
            Iterator other(*this);
            ++*this;
            return other;
        }
        bool operator==(Iterator other) const {
            return m_val == other.m_val;
        }
        bool operator!=(Iterator other) const {
            return m_val != other.m_val;
        }

    private:
        IntTy m_val;
    };

    BitSet() : m_val(0) {}
    explicit BitSet(IntTy val) : m_val(val) {}
    BitSet(std::initializer_list<int> init) {
        m_val = 0;
        for (int bit : init)
            m_val |= (IntTy)1 << bit;
    }

    static BitSet AllTrue(std::size_t count) {
        return BitSet(count == sizeof(IntTy) * 8 ? ~(IntTy)0 : (((IntTy)1 << count) - 1));
    }

    Ref operator[](std::size_t bit) {
        return Ref(this, (IntTy)1 << bit);
    }
    const Ref operator[](std::size_t bit) const {
        return (*const_cast<BitSet*>(this))[bit];
    }
    bool operator==(BitSet other) const {
        return m_val == other.m_val;
    }
    bool operator!=(BitSet other) const {
        return m_val != other.m_val;
    }
    bool operator<(BitSet other) const {
        return m_val < other.m_val;
    }
    bool operator>(BitSet other) const {
        return m_val > other.m_val;
    }
    BitSet operator|(BitSet other) const {
        return BitSet(m_val | other.m_val);
    }
    BitSet operator&(BitSet other) const {
        return BitSet(m_val & other.m_val);
    }
    BitSet operator^(BitSet other) const {
        return BitSet(m_val ^ other.m_val);
    }
    BitSet operator~() const {
        return BitSet(~m_val);
    }
    BitSet& operator|=(BitSet other) {
        return *this = *this | other;
    }
    BitSet& operator&=(BitSet other) {
        return *this = *this & other;
    }
    BitSet& operator^=(BitSet other) {
        return *this = *this ^ other;
    }
    operator u32() = delete;
    operator bool() {
        return m_val != 0;
    }

    // Warning: Even though on modern CPUs this is a single fast instruction,
    // Dolphin's official builds do not currently assume POPCNT support on x86,
    // so slower explicit bit twiddling is generated.  Still should generally
    // be faster than a loop.
    unsigned int Count() const {
        return CountSetBits(m_val);
    }

    Iterator begin() const {
        return Iterator(m_val);
    }
    Iterator end() const {
        return Iterator(0);
    }

    IntTy m_val;
};
