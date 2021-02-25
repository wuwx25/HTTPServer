# HTTP Server
使用Linux c++实现了get和post请求的HTTP服务器。
基于Reactor模型，使用Epoll边沿触发的IO多路复用技术。
实现了线程池，事务管理，定时器，客户请求处理，数据库几大模块。
支持对用户请求的多线程处理和定时处理非活动连接。

