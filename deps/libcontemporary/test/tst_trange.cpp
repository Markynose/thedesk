#include "tst_trange.h"

#include <QtTest>
#include <ranges/trange.h>

TRange::TRange() {
}

TRange::~TRange() {
}

void TRange::trange_canIterate() {
    QList<int> ints({1, 2, 3, 4, 5});
    auto range = tRange(ints);

    auto i = 0;
    for (auto item : range) {
        QCOMPARE(item, ints.at(i));
        i++;
    }
    QCOMPARE(i, ints.length());
}

void TRange::trange_canMap() {
    QList<int> ints({1, 2, 3, 4, 5});
    auto range = tRange(ints).map<int>([](int x) {
        return x * 2;
    });

    auto i = 0;
    for (auto item : range) {
        QCOMPARE(item, ints.at(i) * 2);
        i++;
    }
    QCOMPARE(i, ints.length());
}

void TRange::trange_canMapToDifferentType() {
    QList<int> ints({1, 2, 3, 4, 5});
    auto range = tRange(ints).map<QString>([](int x) {
        return QString::number(x);
    });

    auto i = 0;
    for (auto item : range) {
        QCOMPARE(item, QString::number(ints.at(i)));
        i++;
    }
    QCOMPARE(i, ints.length());
}

void TRange::trange_canMapWithIndex() {
    QList<int> ints({632, 4632, 334, 42, 55});
    auto range = tRange(ints).map<QString>([](int x, int y) {
        return QString::number(y);
    });

    auto i = 0;
    for (auto item : range) {
        QCOMPARE(item, QString::number(i));
        i++;
    }
    QCOMPARE(i, ints.length());
}

void TRange::trange_canFilter() {
    QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto range = tRange(ints).filter([](int x) {
        return x % 2 == 0;
    });

    auto i = 0;
    for (auto item : range) {
        QVERIFY(item % 2 == 0);
        i++;
    }
    QCOMPARE(i, ints.length() / 2);
}

void TRange::trange_canFilterWithIndex() {
    QList<int> ints({2345, 162, 735, 9238, 5234});
    auto range = tRange(ints).filter([](int x, int y) {
        return y == 2;
    });

    auto i = 0;
    for (auto item : range) {
        QCOMPARE(item, ints.at(2));
        i++;
    }
    QCOMPARE(i, 1);
}

void TRange::trange_canCompose() {
    QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto range = tRange(ints).filter([](int x) {
                                 return x % 2 == 0;
                             })
                     .map<QString>([](int x) {
                         return QString::number(x);
                     });

    for (QString item : range) {
        QVERIFY(item.toInt() % 2 == 0);
    }
}

void TRange::trange_canTake() {
    QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto range = tRange(ints).take(3);

    auto i = 1;
    auto count = 0;
    for (auto item : range) {
        QCOMPARE(item, i);
        i++;
        count++;
    }
    QCOMPARE(count, 3);
}

void TRange::trange_canSkip() {
    QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto range = tRange(ints).skip(3);

    auto i = 4;
    auto count = 0;
    for (auto item : range) {
        QCOMPARE(item, i);
        i++;
        count++;
    }
    QCOMPARE(count, 7);
}

void TRange::trange_canUnique() {
    QList<int> ints({1, 1, 2, 3, 3, 3, 4, 4, 5, 6});
    auto range = tRange(ints).unique();

    auto count = 0;
    for (auto item : range) {
        count++;
    }
    QCOMPARE(count, 6);
}

void TRange::trange_canReturn() {
    auto createRange = [] {
        QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        auto range = tRange(ints).filter([](int x) {
                                     return x % 2 == 0;
                                 })
                         .map<QString>([](int x) {
                             return QString::number(x);
                         });
        return range;
    };

    auto range = createRange();
    for (QString item : range) {
        QVERIFY(item.toInt() % 2 == 0);
    }
}

void TRange::trange_canFirst() {
    QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto first = tRange(ints).first();

    QCOMPARE(first, 1);
}

void TRange::trange_canFirstWithFilter() {
    QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto first = tRange(ints).first([](int i) {
        return i > 5;
    });

    QCOMPARE(first, 6);
}

void TRange::trange_canFirstWithNoElements() {
    QList<int> ints;
    QVERIFY_THROWS_EXCEPTION(tRangeException, tRange(ints).first());
}

void TRange::trange_canReduce() {
    QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto result = tRange(ints).reduce<int>([](int next, int previousValue) {
        return previousValue + next;
    });

    QCOMPARE(result, 55);
}

void TRange::trange_canSum() {
    QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto result = tRange(ints).sum();

    QCOMPARE(result, 55);
}

void TRange::trange_canMin() {
    QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto result = tRange(ints).min();

    QCOMPARE(result, 1);
}

void TRange::trange_canMax() {
    QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto result = tRange(ints).max();

    QCOMPARE(result, 10);
}

void TRange::trange_canCast() {
    QList<int> ints({1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    tRange<double> doubles = tRange(ints).staticCast<double>();

    auto count = 0;
    for (auto item : doubles) {
        count++;
    }
    QCOMPARE(count, 10);
}
