#ifndef TRANGE_H
#define TRANGE_H

#include "texception.h"
#include <QCoroGenerator>
#include <QList>
#include <QUuid>
#include <functional>
#include <ranges>
#include <tlogger.h>

class LIBCONTEMPORARY_EXPORT tRangeException : public tException {
        T_EXCEPTION(tRangeException)
};

template<typename T> class tRangeBacking {
    public:
        tRangeBacking() {
        }

        tRangeBacking(QCoro::Generator<const T>&& backing) :
            _backing(std::move(backing)) {
        }

        tRangeBacking(QList<T> list) :
            _backing(gen_list_impl(list)) {
        }

        tRangeBacking(tRangeBacking<T>&) = delete;
        tRangeBacking(tRangeBacking<T>&&) = delete;

        typename QCoro::Generator<const T>::iterator begin() {
            if (_ran) {
                tWarn("tRangeBacking") << "tRangeBacking::begin called more than once";
                return end();
            }
            _ran = true;
            return _backing.begin();
        }

        typename QCoro::Generator<const T>::iterator end() {
            return _backing.end();
        }

        operator QCoro::Generator<const T>() {
            return _backing;
        }

        using BackingType = T;

    private:
        QCoro::Generator<const T> _backing;
        bool _ran = false;

        QCoro::Generator<const T> gen_list_impl(QList<T> list) {
            for (auto i = list.cbegin(); i != list.cend(); i++) {
                co_yield *i;
            }
        }
};

template<typename T, typename R>
concept HasCastFunctions = requires(T t) {
                               { t.template staticCast<R>() } -> std::same_as<QSharedPointer<R>>;
                               { t.template objectCast<R>() } -> std::same_as<QSharedPointer<R>>;
                           };

template<typename T> class tRange {
    public:
        template<typename R> using MapFunction = std::function<const R(T)>;
        template<typename R> using MapFunctionWithIndex = std::function<const R(T, int)>;
        template<typename R> using UniqueFunction = std::function<const R(T)>;
        template<typename R> using ReduceFunction = std::function<const R(T, R)>;
        using FilterFunction = std::function<bool(T)>;
        using FilterFunctionWithIndex = std::function<bool(T, int)>;

        tRange(QList<T> list) :
            _backing(new tRangeBacking<T>(list)) {
        }

        tRange(QCoro::Generator<const T>&& gen) :
            _backing(new tRangeBacking<T>(std::move(gen))) {
        }

        tRange(tRange<T>&& other) {
            _backing = other._backing;
            other._backing.clear();
        }

        tRange(tRangeBacking<T>* backing) :
            _backing(backing) {
        }

        tRange& operator=(tRange<T>&& other) {
            _backing = other._backing;
            other._backing.clear();
            return this;
        }

        template<typename R> tRange<R> map(MapFunction<R> mapping) {
            return tRange<R>(new tRangeBacking(map_impl(mapping, _backing)));
        }

        template<typename R> tRange<R> map(MapFunctionWithIndex<R> mapping) {
            return tRange<R>(new tRangeBacking(map_impl(mapping, _backing)));
        }

        template<typename R> R reduce(ReduceFunction<R> reducing, R initialValue = {}) {
            for (auto item : *_backing) {
                initialValue = reducing(item, initialValue);
            }
            return initialValue;
        }

        tRange<T> filter(FilterFunction filtering) {
            return tRange(new tRangeBacking(filter_impl(filtering, _backing)));
        }

        tRange<T> filter(FilterFunctionWithIndex filtering) {
            return tRange(new tRangeBacking(filter_impl(filtering, _backing)));
        }

        tRange<T> unique() {
            return unique<T>([](T item) {
                return item;
            });
        }

        template<typename R> tRange<T> unique(UniqueFunction<R> uniqing) {
            return tRange(new tRangeBacking(unique_impl(uniqing, _backing)));
        }

        template<typename R> tRange<R> staticCast() {
            return tRange<R>(new tRangeBacking<R>(staticCast_impl<R>(_backing)));
        }

        template<typename R>
            requires HasCastFunctions<T, R>
        tRange<QSharedPointer<R>> staticCast() {
            return tRange<QSharedPointer<R>>(new tRangeBacking<QSharedPointer<R>>(staticCast_impl<R>(_backing)));
        }

        template<typename R> tRange<R> reinterpretCast() {
            return tRange<R>(new tRangeBacking<R>(reinterpretCast_impl(_backing)));
        }

        template<typename R> tRange<R> objectCast() {
            return tRange<R>(new tRangeBacking<R>(objectCast_impl<R>(_backing)));
        }

        template<typename R>
            requires HasCastFunctions<T, R>
        tRange<QSharedPointer<R>> objectCast() {
            return tRange<QSharedPointer<R>>(new tRangeBacking<QSharedPointer<R>>(objectCast_impl<R>(_backing)));
        }

        T sum() {
            return reduce<T>([](T next, T previousValue) {
                return next + previousValue;
            });
        }

        T min() {
            struct MinHelper {
                    T previousValue;
                    bool isFirst;
            };

            auto result = reduce<MinHelper>([](T next, MinHelper previousValue) {
                return MinHelper{
                    previousValue.isFirst ? next : qMin(previousValue.previousValue, next),
                    false};
            },
                {{}, true});
            return result.previousValue;
        }

        T max() {
            struct MaxHelper {
                    T previousValue;
                    bool isFirst;
            };

            auto result = reduce<MaxHelper>([](T next, MaxHelper previousValue) {
                return MaxHelper{
                    previousValue.isFirst ? next : qMax(previousValue.previousValue, next),
                    false};
            },
                {{}, true});
            return result.previousValue;
        }

        bool every(FilterFunction filtering) {
            for (auto item : *_backing) {
                if (!filtering(item)) return false;
            }
            return true;
        }

        bool some(FilterFunction filtering) {
            for (auto item : *_backing) {
                if (filtering(item)) return true;
            }
            return false;
        }

        T first() {
            return this->first([](const T&) {
                return true;
            });
        }

        T first(FilterFunction filtering) {
            for (auto item : *_backing) {
                if (filtering(item)) return item;
            }
            throw tRangeException();
        }

        T first(FilterFunction filtering, T defaultValue) {
            try {
                return first(filtering);
            } catch (tRangeException&) {
                return defaultValue;
            }
        }

        tRange<T> take(uint num) {
            return tRange(new tRangeBacking(take_impl(num, _backing)));
        }

        tRange<T> skip(uint num) {
            return tRange(new tRangeBacking(skip_impl(num, _backing)));
        }

        QList<T> toList() {
            QList<T> list;
            for (auto item : *_backing) {
                list.append(item);
            }
            return list;
        }

        typename QCoro::Generator<const T>::iterator begin() {
            return _backing->begin();
        }

        typename QCoro::Generator<const T>::iterator end() {
            return _backing->end();
        }

        operator QCoro::Generator<const T>() {
            return _backing;
        }

    private:
        using Backing = QSharedPointer<tRangeBacking<T>>;
        Backing _backing = nullptr;

        // The original tRange may go out of scope before these coroutines are called.
        // Preserve the backing in order to be able to iterate over it.

        template<typename R> QCoro::Generator<const R> map_impl(MapFunction<R> mapping, Backing backing) {
            return map_impl<R>([mapping](T item, int index) {
                return mapping(item);
            },
                backing);
        }

        template<typename R> QCoro::Generator<const R> map_impl(MapFunctionWithIndex<R> mapping, Backing backing) {
            auto i = 0;
            for (auto item : *backing) {
                co_yield mapping(item, i);
                i++;
            }
        }

        QCoro::Generator<const T> filter_impl(FilterFunction filtering, Backing backing) {
            return filter_impl([filtering, backing](T item, int index) {
                return filtering(item);
            },
                backing);
        }

        QCoro::Generator<const T> filter_impl(FilterFunctionWithIndex filtering, Backing backing) {
            auto i = 0;
            for (auto item : *backing) {
                if (filtering(item, i)) co_yield item;
                i++;
            }
        }

        template<typename R> QCoro::Generator<const T> unique_impl(UniqueFunction<R> uniqing, Backing backing) {
            QSet<R> found;
            for (auto item : *backing) {
                auto uniqueIdx = uniqing(item);
                if (!found.contains(uniqueIdx)) {
                    co_yield item;
                    found.insert(uniqueIdx);
                }
            }
        }

        QCoro::Generator<const T> take_impl(uint num, Backing backing) {
            auto i = 0;
            for (auto item : *backing) {
                co_yield item;
                i++;
                if (i == num) co_return;
            }
        }

        QCoro::Generator<const T> skip_impl(uint num, Backing backing) {
            auto i = 0;
            for (auto item : *backing) {
                i++;
                if (i > num) {
                    co_yield item;
                }
            }
        }

        template<typename R> QCoro::Generator<const R> staticCast_impl(Backing backing) {
            for (auto item : *backing) {
                co_yield static_cast<R>(item);
            }
        }

        template<typename R>
            requires HasCastFunctions<T, R>
        QCoro::Generator<const QSharedPointer<R>> staticCast_impl(Backing backing) {
            for (auto item : *backing) {
                co_yield item.template staticCast<R>();
            }
        }

        template<typename R> QCoro::Generator<const R> reinterpretCast_impl(Backing backing) {
            for (auto item : *backing) {
                co_yield reinterpret_cast<R>(item);
            }
        }

        template<typename R> QCoro::Generator<const R> objectCast_impl(Backing backing) {
            for (auto item : *backing) {
                co_yield qobject_cast<R>(item);
            }
        }

        template<typename R>
            requires HasCastFunctions<T, R>
        QCoro::Generator<const QSharedPointer<R>> objectCast_impl(Backing backing) {
            for (auto item : *backing) {
                co_yield item.template objectCast<R>();
            }
        }
};

#endif // TRANGE_H
