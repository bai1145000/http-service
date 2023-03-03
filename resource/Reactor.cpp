#include <iostream>
#include <map>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <errno.h>
#include <cstdlib>
#include "headers/Reactor.h"

using namespace std;

#define MAX_EVENTS 1024
#define MAXLINE 1024

class Connection : public EventHandler
{

private:
    int fd;

public:
    Connection(int sockfd) : fd(sockfd)
    {
        // 设置非阻塞模式
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    }

    ~Connection()
    {
        cout << fd << "close" << endl;
        close(fd);
    }

    virtual void handleRead()
    {
        char buf[MAXLINE];
        ssize_t n = read(fd, buf, MAX_EVENTS); // 读取fd的MAX_EVENTS字节数据到buf中
        cout << "read data ok .." << endl;
        if (n == 0)
        {
            cout << "client close" << endl;
            shutdown(fd, 0);
            close(fd);
            fd = -1;
        }
        else if (n < 0)
        {
            // EINTR：系统调用被信号中断。
            // EWOULDBLOCK 或 EAGAIN：操作非阻塞，但请求的操作会阻塞（例如在等待一个连接时），或者没有更多数据可读。
            if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN)
            {
                return;
            }
            else
            {
                cout << "read error" << endl;
                // close(fd);
                // delete this;
            }
        }
        else
        {
            cout << "recv: \r\n"
                 << buf << endl;
        }
    }

    virtual void handleWrite()
    {
        char buf[] = "handleWrite .....";
        ssize_t n = write(fd, buf, strlen(buf));
        if (n == -1)
        {
            cout << "write error" << endl;
        }

        // 写完数据就关闭客户端连接
        // close(fd);
        // delete this;
    }

    virtual int getFd()
    {
        return fd;
    }
};

class EventLoop
{

private:
    int epoll_fd;

public:
    EventLoop() : epoll_fd(epoll_create(1))
    {

        if (epoll_fd == -1)
        {
            perror("epoll_create");
            exit(EXIT_FAILURE);
        }
    };

    ~EventLoop()
    {

        close(epoll_fd);
    }

    void registerEventHandler(EventHandler *eventHandler)
    {
        struct epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.ptr = eventHandler;
        // event.data.fd = eventHandler->getFd();
        cout << "epoll_fd " << epoll_fd << "  add fd" << eventHandler->getFd() << endl;
        // 将需要监听的fd放到epoll缓存区中
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, eventHandler->getFd(), &event) < 0)
        {
            cout << "epoll_ctl error" << endl;
            close(eventHandler->getFd());
        }
    }

    void eventLopp(int server_fd)
    {

        struct epoll_event events[MAX_EVENTS];
        AcceptConnection(server_fd);
        while (true)
        {
            // 等待要epoll_fd上注册的fd的事件发生
            cout << "epoll_wait .." << endl;
            int nf = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
            if (nf == -1)
            {
                if (errno == EINTR)
                {
                    cout << "epoll_wait EINTR" << endl;
                    continue;
                }
                else
                {
                    cout << "epoll_wait error" << endl;
                    exit(1);
                }
            }
            cout << "handler cout : " << nf << endl;
            for (int i = 0; i < nf; i++)
            {
                Connection *eventHandler = (Connection *)(events[i].data.ptr);
                if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP)
                {
                    cout << events[i].data.fd << "error .." << endl;
                    close(events[i].data.fd);
                    continue;
                }
                else if (server_fd == events[i].data.fd)
                {
                    while (true)
                    {
                        // 等待链接事件
                        cout << events[i].data.fd << " wait ....." << endl;
                        int confd = AcceptConnection(server_fd);
                        if (confd == -1)
                        {
                            break;
                        }
                    }
                }
                else
                {
                    cout << events[i].data.fd << " handler ...." << endl;
                    int evfd = eventHandler->getFd();
                    if (events[i].events & EPOLLIN)
                    {
                        cout << events[i].data.fd << " read....." << endl;
                        eventHandler->handleRead();
                        // eventHandler->handleWrite(); // 接受完成之后，立即发送一条消息

                        // close(events[i].data.fd); // 触发一个读取事件
                        // close(eventHandler->getFd());
                        // epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL); // 删除之后，后续都接受不了
                    }
                    if (events[i].events & EPOLLOUT)
                    {
                        cout << events[i].data.fd << " write....." << endl;
                        eventHandler->handleWrite();
                    }

                    if (eventHandler->getFd() == -1)
                    {
                        cout << "epoll_fd del " << events[i].data.fd << endl;
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                    }

                    // close(events[i].data.fd);
                    cout << "for i " << events[i].data.fd << endl
                         << "eventHandler->getFd() " << eventHandler->getFd() << endl;
                }
            }
        }
    }

    int AcceptConnection(int listen_fd_)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int conn_fd = accept(listen_fd_, (struct sockaddr *)&client_addr, &addr_len);
        if (conn_fd == -1)
        {
            std::cout << "accept failed, errno= " << errno << std::endl;
            return -1;
        }
        cout << conn_fd << "conection succeed ...." << endl;
        int flags = fcntl(conn_fd, F_GETFL, 0);
        EventHandler *conn = new Connection(conn_fd);
        registerEventHandler(conn);
        return conn_fd;
    }
};