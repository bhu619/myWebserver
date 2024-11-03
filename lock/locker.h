#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

/* 信号量类 */
class sem {
public:

    /* 构造函数 - 默认信号量值为 0 */
    sem() {
        if (sem_init(&m_sem, 0, 0) != 0) {
            throw std::runtime_error("Failed to initialize semaphore");
        }
    }

    /* 构造函数 - 可以设置初始信号量值 */
    sem(int num) {
        if (sem_init(&m_sem, 0, num) != 0) {
            throw std::runtime_error("Failed to initialize semaphore");
        }
    }

    ~sem() {
        sem_destroy(&m_sem);
    }

    bool wait() { return sem_wait(&m_sem) == 0; }   /* 等待信号量，类似于 P 操作，将信号量的值 - 1 */
    bool post() { return sem_post(&m_sem) == 0; }   /* 释放信号量，类似于 V 操作，将信号量的值 + 1 */

private:
    sem_t m_sem;
};

/* 琐 */
class locker {
public:
    locker() {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }

    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock() { return pthread_mutex_lock(&m_mutex) == 0; }

    bool unlock() { return pthread_mutex_unlock(&m_mutex) == 0; }

    pthread_mutex_t *get() { return &m_mutex; }

private:
    pthread_mutex_t m_mutex;
};

/* 与 std::lock_guard 类似的 RAII 风格的锁 */
class locker_guard {
public:
    explicit locker_guard(locker &l) : m_locker(l) {
        m_locker.lock();        /* 在构造时加锁 */
    }
    
    ~locker_guard() {
        m_locker.unlock();      /* 在析构时解锁 */
    }

    // 禁用拷贝构造和赋值操作
    locker_guard(const locker_guard &) = delete;
    locker_guard &operator=(const locker_guard &) = delete;

private:
    locker &m_locker;  // 引用自定义锁
};

/* 条件变量 */
class cond {
public:
    cond() {
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            //pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }

    ~cond() {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *m_mutex) {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_wait(&m_cond, m_mutex);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }

    bool timewait(pthread_mutex_t *m_mutex, struct timespec t) {
        int ret = 0;
        //pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        //pthread_mutex_unlock(&m_mutex);
        return ret == 0;
    }

    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }

    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    //static pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};

#endif
