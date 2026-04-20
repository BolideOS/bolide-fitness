/*
 * Copyright (C) 2026 BolideOS Contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <QVector>
#include <algorithm>
#include <cstring>

/**
 * @brief Fixed-capacity ring buffer optimized for hot-path sensor data.
 *
 * - No dynamic allocation after construction.
 * - O(1) push, random-access by reverse index (0 = newest).
 * - Supports bulk push for batched sensor reads.
 * - Provides contiguous-copy helpers for DSP / metrics.
 *
 * Template parameter T should be trivially copyable for best performance
 * (float, double, SensorSample structs, etc.).
 */
template<typename T>
class RingBuffer
{
public:
    explicit RingBuffer(int capacity = 0)
        : m_capacity(capacity)
        , m_head(0)
        , m_count(0)
    {
        if (m_capacity > 0)
            m_data.resize(m_capacity);
    }

    // ── Capacity ────────────────────────────────────────────────────────

    int  count()    const { return m_count; }
    int  capacity() const { return m_capacity; }
    bool isEmpty()  const { return m_count == 0; }
    bool isFull()   const { return m_count == m_capacity; }

    void reserve(int newCapacity)
    {
        if (newCapacity <= m_capacity) return;
        QVector<T> tmp(newCapacity);
        // Copy existing elements in chronological order
        for (int i = 0; i < m_count; ++i)
            tmp[i] = oldest(i);
        m_data.swap(tmp);
        m_head     = m_count;
        m_capacity = newCapacity;
    }

    void clear()
    {
        m_head  = 0;
        m_count = 0;
    }

    // ── Single-element access ───────────────────────────────────────────

    /** Push one element (overwrites oldest when full). */
    void push(const T &value)
    {
        m_data[m_head] = value;
        m_head = (m_head + 1) % m_capacity;
        if (m_count < m_capacity)
            ++m_count;
    }

    /** Access by reverse index: 0 = newest, count()-1 = oldest. */
    const T &at(int reverseIndex) const
    {
        int idx = (m_head - 1 - reverseIndex + m_capacity) % m_capacity;
        return m_data[idx];
    }

    /** Access by forward (chronological) index: 0 = oldest. */
    const T &oldest(int forwardIndex) const
    {
        int start = (m_head - m_count + m_capacity) % m_capacity;
        int idx   = (start + forwardIndex) % m_capacity;
        return m_data[idx];
    }

    const T &newest() const { return at(0); }

    // ── Bulk operations ─────────────────────────────────────────────────

    /** Push N contiguous elements (oldest first). */
    void pushBulk(const T *src, int n)
    {
        for (int i = 0; i < n; ++i)
            push(src[i]);
    }

    /**
     * Copy the last @p n elements (chronological order) into @p dst.
     * Returns actual number copied (may be < n if count < n).
     */
    int copyLast(T *dst, int n) const
    {
        int toCopy = qMin(n, m_count);
        int startReverse = toCopy - 1;
        for (int i = 0; i < toCopy; ++i)
            dst[i] = at(startReverse - i);
        return toCopy;
    }

    /**
     * Copy all elements in chronological order into @p dst.
     * @p dst must hold at least count() elements.
     */
    int copyAll(T *dst) const
    {
        return copyLast(dst, m_count);
    }

    // ── Statistical helpers (for numeric T) ─────────────────────────────

    /** Mean of last n elements. */
    T mean(int n = -1) const
    {
        int len = (n < 0 || n > m_count) ? m_count : n;
        if (len == 0) return T(0);
        T sum = T(0);
        for (int i = 0; i < len; ++i)
            sum += at(i);
        return sum / static_cast<T>(len);
    }

    /** Max of last n elements. */
    T max(int n = -1) const
    {
        int len = (n < 0 || n > m_count) ? m_count : n;
        if (len == 0) return T(0);
        T result = at(0);
        for (int i = 1; i < len; ++i) {
            if (at(i) > result) result = at(i);
        }
        return result;
    }

    /** Min of last n elements. */
    T min(int n = -1) const
    {
        int len = (n < 0 || n > m_count) ? m_count : n;
        if (len == 0) return T(0);
        T result = at(0);
        for (int i = 1; i < len; ++i) {
            if (at(i) < result) result = at(i);
        }
        return result;
    }

private:
    QVector<T> m_data;
    int m_capacity;
    int m_head;
    int m_count;
};

#endif // RINGBUFFER_H
