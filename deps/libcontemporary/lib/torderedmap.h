//
// Created by Victor Tran on 17/4/2022.
//

#ifndef LIBCONTEMPORARY_TORDEREDMAP_H
#define LIBCONTEMPORARY_TORDEREDMAP_H

#include <QList>
#include <QPair>

template <typename K, typename V> class tOrderedMap {
private:
    typedef QPair<K, V> MapPair;
    typedef typename QList<MapPair>::iterator iterator;
    typedef typename QList<MapPair>::const_iterator const_iterator;

public:
    void append(K key, V value) {
        items.append({key, value});
    }

    V value(K key, V defaultValue = V()) {
        for (MapPair item : items) {
            if (item.first == key) return item.second;
        }
        return defaultValue;
    }

    K key(V value, K defaultKey = K()) {
        for (MapPair item : items) {
            if (item.second == value) return item.first;
        }
        return defaultKey;
    }

    bool contains(K key) {
        for (MapPair item : items) {
            if (item.first == key) return true;
        }
        return false;
    }

    void remove(K key) {
        for (auto i = items.begin(); i != items.end(); i++) {
            if (i->first == key) {
                items.remove(i);
                return;
            }
        }
    }

    bool isEmpty() {
        return items.isEmpty();
    }

    iterator begin() {
        return items.begin();
    }

    const_iterator begin() const {
        return items.cbegin();
    }

    iterator end() {
        return items.end();
    }

    const_iterator end() const {
        return items.cend();
    }

private:
    QList<MapPair> items;
};

#endif//LIBCONTEMPORARY_TORDEREDMAP_H
