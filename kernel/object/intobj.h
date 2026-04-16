/**
 * @file intobj.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 测试对象, 用于测试能力系统
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <kio.h>
#include <mem/alloc.h>
#include <mem/slub.h>
#include <object/shared.h>
#include <perm/intobj.h>
#include <sustcore/capability.h>

class IntObjOperator;
class SharedIntObjOperator;

class IntObject : public _PayloadHelper<IntObject> {
public:
    static constexpr PayloadType IDENTIFIER = PayloadType::INTOBJ;
    friend class IntObjOperator;
    using Operation = IntObjOperator;

private:
    int value;

public:
    constexpr IntObject(int v) : value(v) {}
    ~IntObject() = default;
protected:
    int _read() const {
        return value;
    }
    void _write(int v) {
        value = v;
    }
    void _increase() {
        value++;
    }
    void _decrease() {
        value--;
    }
};

class SharedIntObjectManager;

class SharedIntObject : public SharedObject<SharedIntObject> {
public:
    static constexpr PayloadType IDENTIFIER = PayloadType::SINTOBJ;
    friend class SharedIntObjOperator;
    friend class SharedIntObjectManager;
    using Operation = SharedIntObjOperator;

private:
    int value;
    bool discarded = false;

public:
    constexpr SharedIntObject(int v) : value(v) {}
    virtual ~SharedIntObject() = default;

    virtual void on_death() {
        discarded = true;
    }

protected:
    int _read() const {
        return value;
    }
    void _write(int v) {
        value = v;
    }
    void _increase() {
        value++;
    }
    void _decrease() {
        value--;
    }
};

class SharedIntObjectManager {
protected:
    util::ArrayList<SharedIntObject *> objects;

public:
    constexpr SharedIntObjectManager() : objects() {}
    ~SharedIntObjectManager() {
        for (SharedIntObject *obj : objects) {
            assert(obj->discarded == false);
            delete obj;
        }
    }

    void GC() {
        for (auto &obj : objects) {
            if (obj->discarded) {
                delete obj;
                obj = nullptr;
            }
        }

        auto it = objects.find(nullptr);
        while (it != objects.end()) {
            objects.erase(it);
            it = objects.find(nullptr);
        }
    }

    template <typename... Args>
    SharedIntObject *create(Args... args) {
        SharedIntObject *obj = new SharedIntObject(args...);
        objects.push_back(obj);
        return obj;
    }

    constexpr size_t object_count() const {
        return objects.size();
    }
};

using SharedIntObjectAccessor = SharedObjectAccessor<SharedIntObject>;

class IntObjOperator {
protected:
    Capability *_cap;
    IntObject *_obj;

    template <b64 perm>
    bool imply() const {
        return _cap->perm().basic_imply(perm);
    }

public:
    constexpr IntObjOperator(Capability *cap)
        : _cap(cap), _obj(cap->payload<IntObject>()) {}
    ~IntObjOperator() = default;

    void *operator new(size_t size) = delete;
    void operator delete(void *ptr) = delete;

    Result<int> read() const;
    Result<void> write(int v);
    Result<void> increase();
    Result<void> decrease();
};

class SharedIntObjOperator {
protected:
    Capability *_cap;
    SharedIntObjectAccessor *_acc;
    SharedIntObject *_obj;
    template <b64 perm>
    bool imply() const {
        return _cap->perm().basic_imply(perm);
    }

public:
    constexpr SharedIntObjOperator(Capability *cap)
        : _cap(cap), _acc(cap->payload<SharedIntObjectAccessor>()), _obj(_acc->obj()) {}
    ~SharedIntObjOperator() = default;

    void *operator new(size_t size) = delete;
    void operator delete(void *ptr) = delete;

    Result<int> read() const;
    Result<void> write(int v);
    Result<void> increase();
    Result<void> decrease();
};