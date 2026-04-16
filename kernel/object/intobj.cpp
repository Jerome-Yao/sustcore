/**
 * @file intobj.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 测试对象, 用于测试能力系统
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/capability.h>
#include <object/csa.h>
#include <object/intobj.h>
#include <sustcore/errcode.h>

Result<int> IntObjOperator::read() const {
    using namespace perm::intobj;
    if (!imply<READ>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    return _obj->_read();
}

Result<void> IntObjOperator::write(int v) {
    using namespace perm::intobj;
    if (!imply<WRITE>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_write(v);
    return {};
}

Result<void> IntObjOperator::increase() {
    using namespace perm::intobj;
    if (!imply<INCREASE>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_increase();
    return {};
}

Result<void> IntObjOperator::decrease() {
    using namespace perm::intobj;
    if (!imply<DECREASE>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_decrease();
    return {};
}

Result<int> SharedIntObjOperator::read() const {
    using namespace perm::sintobj;
    if (!imply<READ>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    return _obj->_read();
}

Result<void> SharedIntObjOperator::write(int v) {
    using namespace perm::sintobj;
    if (!imply<WRITE>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_write(v);
    return {};
}

Result<void> SharedIntObjOperator::increase() {
    using namespace perm::sintobj;
    if (!imply<INCREASE>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_increase();
    return {};
}

Result<void> SharedIntObjOperator::decrease() {
    using namespace perm::sintobj;
    if (!imply<DECREASE>()) {
        loggers::CAPABILITY::ERROR("权限不足");
        return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
    }
    _obj->_decrease();
    return {};
}