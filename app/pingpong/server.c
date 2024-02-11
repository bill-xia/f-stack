#include "ff_api.h"
#include "ff_epoll.h"
#include <sys/socket.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#define MAX(a, b)           \
    __extension__({         \
        typeof(a) _a = (a); \
        typeof(b) _b = (b); \
        _a > _b ? _a : _b;  \
    })

#define MIN(a, b)           \
    __extension__({         \
        typeof(a) _a = (a); \
        typeof(b) _b = (b); \
        _a < _b ? _a : _b;  \
    })

char content[] = "hello, world!\n";
const int CONTENT_LENGTH = sizeof(content) - 1;

#define SEND_BUF_SIZE 30000000
char app_recv_buf[SEND_BUF_SIZE];
char app_send_buf[SEND_BUF_SIZE];
int epfd, listen_fd;
int read_bytes, written_bytes;

const int N_ROUNDS = 100;
int n_rounds = 0;

void onconn(int fd);
void onread(int fd, int ret);

int pingpong_main(void *arg)
{
    static struct epoll_event evts[4096];
    int n = ff_epoll_wait(epfd, &evts, 4096, 10);
    for (int i = 0; i < n; ++i)
    {
        struct epoll_event *evt = evts + i;
        int fd = evt->data.fd;
        if (fd == listen_fd && evt->events & EPOLLIN)
        {
            struct sockaddr_in addr;
            int nfd = ff_accept(fd, &addr, sizeof(addr));
            continue;
        }
        if (evt->events & EPOLLIN)
        {
            int r = ff_read(fd, app_recv_buf + read_bytes,
                            sizeof(app_send_buf) - read_bytes);
            read_bytes += r;
            if (read_bytes == sizeof(app_send_buf))
            {
                app_recv_buf[20] = '\0';
                printf("Client read %d bytes: %s...\n", read_bytes, app_recv_buf);
                struct timespec tp;
                clock_gettime(CLOCK_REALTIME, &tp);
                long ms = tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
                printf("%018ld one round\n", ms);
                fflush(stdout);
                n_rounds += 1;
                if (n_rounds < N_ROUNDS)
                {
                    read_bytes = 0;
                }
            }
        }
        if (evt->events & EPOLLOUT)
        {
            if (read_bytes == sizeof(app_send_buf))
            {
                int r = ff_write(fd, app_send_buf + written_bytes, sizeof(app_send_buf) - written_bytes);
                written_bytes += r;
                if (written_bytes == sizeof(app_send_buf))
                {
                    written_bytes = 0;
                    read_bytes = 0;
                    if (n_rounds >= N_ROUNDS)
                    {
                        ff_close(fd);
                    }
                }
            }
        }
    }
}

int main()
{
    ff_init(0, NULL); // TODO: conf, argc and argv
    int filled = 0;
    while (filled < SEND_BUF_SIZE)
    {
        int n_bytes = MIN(SEND_BUF_SIZE - filled, CONTENT_LENGTH);
        memcpy(app_send_buf + filled, content, CONTENT_LENGTH);
        filled += n_bytes;
    }
    read_bytes = 0;
    listen_fd = ff_socket(AF_INET, SOCK_STREAM, 0);
    ff_fcntl(listen_fd, F_SETFL, O_NONBLOCK);
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = 1000,
    };
    inet_pton(AF_INET, "127.0.0.1", &(server_addr.sin_addr));
    ff_bind(listen_fd, (struct linux_sockaddr *)&server_addr, sizeof(struct sockaddr_in));
    ff_listen(listen_fd, 1024);
    epfd = ff_epoll_create(1024);
    struct epoll_event epev = {
        .events = EPOLLIN | EPOLLOUT,
        .data = {
            .fd = listen_fd}};
    ff_epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &epev);
    ff_run(pingpong_main, NULL);
    return 0;
}