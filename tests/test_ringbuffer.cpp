/*
 * test_ringbuffer.cpp – Unit tests for RingBuffer<T>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QtTest>
#include "core/ringbuffer.h"

class TestRingBuffer : public QObject
{
    Q_OBJECT

private slots:
    void testEmpty()
    {
        RingBuffer<int> rb(8);
        QVERIFY(rb.isEmpty());
        QCOMPARE(rb.count(), 0);
        QCOMPARE(rb.capacity(), 8);
        QVERIFY(!rb.isFull());
    }

    void testPushAndAt()
    {
        RingBuffer<int> rb(4);
        rb.push(10);
        rb.push(20);
        rb.push(30);

        QCOMPARE(rb.count(), 3);
        QCOMPARE(rb.at(0), 30);  // most recent
        QCOMPARE(rb.at(1), 20);
        QCOMPARE(rb.at(2), 10);  // oldest
    }

    void testOverwrite()
    {
        RingBuffer<int> rb(3);
        rb.push(1);
        rb.push(2);
        rb.push(3);
        QVERIFY(rb.isFull());
        QCOMPARE(rb.count(), 3);

        rb.push(4);  // overwrites 1
        QCOMPARE(rb.count(), 3);
        QCOMPARE(rb.at(0), 4);
        QCOMPARE(rb.at(1), 3);
        QCOMPARE(rb.at(2), 2);
    }

    void testOldest()
    {
        RingBuffer<int> rb(4);
        rb.push(10);
        rb.push(20);
        rb.push(30);
        QCOMPARE(rb.oldest(), 10);

        rb.push(40);
        rb.push(50);  // overwrites 10
        QCOMPARE(rb.oldest(), 20);
    }

    void testClear()
    {
        RingBuffer<float> rb(8);
        rb.push(1.0f);
        rb.push(2.0f);
        QCOMPARE(rb.count(), 2);

        rb.clear();
        QVERIFY(rb.isEmpty());
        QCOMPARE(rb.count(), 0);
    }

    void testMean()
    {
        RingBuffer<float> rb(8);
        rb.push(10.0f);
        rb.push(20.0f);
        rb.push(30.0f);

        float m = rb.mean(3);
        QCOMPARE(m, 20.0f);

        // Mean of last 2
        float m2 = rb.mean(2);
        QCOMPARE(m2, 25.0f);
    }

    void testMax()
    {
        RingBuffer<int> rb(8);
        rb.push(5);
        rb.push(15);
        rb.push(10);
        rb.push(3);

        QCOMPARE(rb.max(4), 15);
        QCOMPARE(rb.max(2), 10);  // last 2: 3, 10
    }

    void testMin()
    {
        RingBuffer<int> rb(8);
        rb.push(5);
        rb.push(15);
        rb.push(10);
        rb.push(3);

        QCOMPARE(rb.min(4), 3);
        QCOMPARE(rb.min(3), 3);   // last 3: 3, 10, 15
    }

    void testBulkPush()
    {
        RingBuffer<int> rb(8);
        int data[] = {1, 2, 3, 4, 5};
        rb.pushBulk(data, 5);

        QCOMPARE(rb.count(), 5);
        QCOMPARE(rb.at(0), 5);
        QCOMPARE(rb.at(4), 1);
    }

    void testToVector()
    {
        RingBuffer<int> rb(4);
        rb.push(10);
        rb.push(20);
        rb.push(30);

        QVector<int> v = rb.toVector();
        QCOMPARE(v.size(), 3);
        // Oldest first
        QCOMPARE(v[0], 10);
        QCOMPARE(v[1], 20);
        QCOMPARE(v[2], 30);
    }

    void testLargeSequence()
    {
        RingBuffer<int> rb(64);
        for (int i = 0; i < 1000; ++i)
            rb.push(i);

        QCOMPARE(rb.count(), 64);
        QCOMPARE(rb.at(0), 999);
        QCOMPARE(rb.oldest(), 936);
    }
};

QTEST_MAIN(TestRingBuffer)
#include "test_ringbuffer.moc"
