# 数据库连接池

## `connection_pool` 类  
`sql_connection_pool.h`中的`connection_pool` 类实现了一个 **数据库连接池**，用于管理数据库连接。它采用了 **单例模式** 来确保连接池在整个程序运行期间只有一个实例。
设置数据库连接池的目的是为了提高性能和减少数据库连接建立和销毁的开销。  
1. `GetInstance()`：实现单例模式，通过静态成员函数获取唯一的连接池实例。
2. 使用信号量 `reserve` 控制连接的获取和释放，确保线程在连接不可用时会阻塞，直到有可用连接。
3. 使用 互斥锁 用于多线程同步，保护对 `connList` 和连接计数器的操作，确保线程安全。
4. 使用 `std::list<std::shared_ptr<MYSQL>>` 来保存连接池中的所有数据库连接。`shared_ptr` 用于自动管理连接的生命周期，确保资源在适当时被释放。
5. `connectionRAII` 实现了 **RAII（资源获取即初始化）** 的模式，用于自动管理数据库连接的获取和释放。

## `connectionRAII` 类
`connectionRAII` 类是一个典型的 RAII 模式实现，用于管理数据库连接的生命周期。它的作用是确保从连接池中获取的数据库连接能够在使用结束后自动释放，从而防止资源泄漏。保证每次从连接池中获取的连接在使用后都被正确归还。通过 RAII 模式，数据库连接的生命周期与 `connectionRAII` 对象绑定，确保不会有连接被无意遗忘。 

## 笔记
[5. 数据库连接池实现——语雀](https://www.yuque.com/u39624144/zvaea9/cf61o7ekwp4414fh?singleDoc#)

[5. 数据库连接池实现——CSDN](https://blog.csdn.net/Teriri_/article/details/143376885?spm=1001.2014.3001.5502)
