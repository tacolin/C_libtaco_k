#include "kudp.h"

#define derror(a, b...) printk("[ERROR] %s(): "a"\n", __func__, ##b)
#define CHECK_IF(assertion, error_action, ...) \
{\
    if (assertion) \
    { \
        derror(__VA_ARGS__); \
        {error_action;} \
    }\
}

static struct socket* _socket(int family, int type, int protocol)
{
    struct socket* s;
    struct file* fp;
    int chk;

    chk = sock_create(family, type, protocol, &s);
    CHECK_IF(chk < 0, return NULL, "sock_create failed");
    CHECK_IF(s == NULL, return NULL, "sock_create failed");

    fp = sock_alloc_file(s, 0, NULL);
    if (fp == NULL)
    {
        sock_release(s);
        return NULL;
    }
    return s;
}

static int _inet_pton(int af, const char* src, void* dst)
{
    CHECK_IF(src == NULL, return UDP_FAIL, "src is null");
    CHECK_IF(dst == NULL, return UDP_FAIL, "dst is null");

    if (af == AF_INET)
    {
        return in4_pton(src, strlen(src), (u8*)dst, '\0', NULL);
    }
    else if (af == AF_INET6)
    {
        return in6_pton(src, strlen(src), (u8*)dst, '\0', NULL);
    }
    else
    {
        derror("unknown af = %d", af);
        return UDP_FAIL;
    }
}

static char* _inet_ntop(int af, void* src, char* buffer, int bufsize)
{
    u8* ptr = (u8*)src;
    CHECK_IF(src == NULL, return NULL, "src is null");
    CHECK_IF(buffer == NULL, return NULL, "buffer is null");
    CHECK_IF(bufsize <= 0, return NULL, "bufsize = %d invalid", bufsize);

    snprintf(buffer, bufsize, "%d.%d.%d.%d", ptr[0], ptr[1], ptr[2], ptr[3]);
    return buffer;
}

static int _sendto(struct socket* s, void* data, int datalen, struct sockaddr_in* remote)
{
    struct msghdr msg       = {};
    struct iovec  iov       = {};

    CHECK_IF(s == NULL, return UDP_FAIL, "s is null");
    CHECK_IF(remote == NULL, return UDP_FAIL, "remote is null");
    CHECK_IF(data == NULL, return UDP_FAIL, "data is null");
    CHECK_IF(datalen <= 0, return UDP_FAIL, "datalen = %d invalid", datalen);

    iov.iov_base       = data;
    iov.iov_len        = datalen;
    msg.msg_flags      = 0;
    msg.msg_name       = remote;
    msg.msg_namelen    = sizeof(struct sockaddr_in);
    msg.msg_control    = NULL;
    msg.msg_controllen = 0;
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;

    return sock_sendmsg(s, &msg, datalen);
}

static int _recvfrom(struct socket* s, void* buffer, int bufsize, struct sockaddr_in* remote)
{
    struct msghdr msg       = {};
    struct iovec  iov       = {};
    int recvlen;

    CHECK_IF(s == NULL, return UDP_FAIL, "s is null");
    CHECK_IF(buffer == NULL, return UDP_FAIL, "buffer is null");
    CHECK_IF(bufsize <= 0, return UDP_FAIL, "bufsize = %d invalid", bufsize);
    CHECK_IF(remote == NULL, return UDP_FAIL, "remote is null");

    iov.iov_base       = buffer;
    iov.iov_len        = bufsize;
    msg.msg_flags      = 0;
    msg.msg_name       = remote;
    msg.msg_namelen    = sizeof(struct sockaddr_in);
    msg.msg_control    = NULL;
    msg.msg_controllen = 0;
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;

    recvlen = sock_recvmsg(s, &msg, bufsize, msg.msg_flags);
    return recvlen;
}

int udp_to_sockaddr(struct udp_addr udp_addr, struct sockaddr_in* sock_addr)
{
    int chk;

    CHECK_IF(sock_addr == NULL, return UDP_FAIL, "sock_addr is null");

    chk = _inet_pton(AF_INET, udp_addr.ipv4, &sock_addr->sin_addr);
    CHECK_IF(chk != 1, return UDP_FAIL, "inet_pton failed");

    sock_addr->sin_family = AF_INET;
    sock_addr->sin_port   = htons(udp_addr.port);
    return UDP_OK;
}

int udp_to_udpaddr(struct sockaddr_in sock_addr, struct udp_addr* udp_addr)
{
    CHECK_IF(udp_addr == NULL, return UDP_FAIL, "udp_addr is null");

    _inet_ntop(AF_INET, &sock_addr.sin_addr, udp_addr->ipv4, INET_ADDRSTRLEN);
    udp_addr->port = ntohs(sock_addr.sin_port);
    return UDP_OK;
}

int udp_init(struct udp* udp, char* local_ip, int local_port)
{
    int chk;
    const int on = 1;
    int len = sizeof(on);
    struct sockaddr_in me = {};
    mm_segment_t oldfs = get_fs();

    CHECK_IF(udp == NULL, return UDP_FAIL, "udp is null");

    set_fs(get_ds());

    udp->local_port = local_port;
    udp->fd = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CHECK_IF(udp->fd == NULL, goto _ERROR, "socket failed");

    if (udp->local_port != UDP_PORT_ANY)
    {
        me.sin_family = AF_INET;
        if (local_ip)
        {
            chk = _inet_pton(AF_INET, local_ip, &me.sin_addr);
            CHECK_IF(chk != 1, goto _ERROR, "inet_pton failed");
        }
        else
        {
            me.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        me.sin_port = htons(local_port);

        chk = sock_setsockopt(udp->fd, SOL_SOCKET, SO_REUSEADDR, (void*)&on, len);
        CHECK_IF(chk < 0, goto _ERROR, "setsockopt reuse failed, chk = %d", chk);

        chk = udp->fd->ops->bind(udp->fd, (struct sockaddr*)&me, sizeof(me));
        CHECK_IF(chk < 0, goto _ERROR, "bind failed");
    }
    udp->is_init = 1;
    set_fs(oldfs);
    return UDP_OK;

_ERROR:
    if (udp->fd)
    {
        sock_release(udp->fd);
        udp->fd = NULL;
    }
    set_fs(oldfs);
    return UDP_FAIL;
}

int udp_uninit(struct udp* udp)
{
    mm_segment_t oldfs = get_fs();

    CHECK_IF(udp == NULL, return UDP_FAIL, "udp is null");
    CHECK_IF(udp->fd == NULL, return UDP_FAIL, "udp->fd is null");
    CHECK_IF(udp->is_init != 1, return UDP_FAIL, "udp is not initialized yet");

    set_fs(get_ds());

    sock_release(udp->fd);
    udp->fd = NULL;

    set_fs(oldfs);
    return UDP_OK;
}

int udp_send(struct udp* udp, struct udp_addr remote, void* data, int data_len)
{
    int chk;
    struct sockaddr_in remote_addr = {};
    mm_segment_t oldfs = get_fs();
    int sendlen;

    CHECK_IF(udp == NULL, return UDP_FAIL, "udp is null");
    CHECK_IF(udp->fd == NULL, return UDP_FAIL, "udp->fd is null");
    CHECK_IF(udp->is_init != 1, return UDP_FAIL, "udp is not initialized yet");
    CHECK_IF(data == NULL, return UDP_FAIL, "data is null");
    CHECK_IF(data_len <= 0, return UDP_FAIL, "data_len is %d", data_len);

    chk = udp_to_sockaddr(remote, &remote_addr);
    CHECK_IF(chk != UDP_OK, return -1, "udp_to_sockaddr failed");

    set_fs(get_ds());
    sendlen = _sendto(udp->fd, data, data_len, &remote_addr);
    set_fs(oldfs);

    return sendlen;
}

int udp_recv(struct udp* udp, void* buffer, int buffer_size, struct udp_addr* remote)
{
    struct sockaddr_in remote_addr = {};
    int chk;
    int recvlen;
    mm_segment_t oldfs = get_fs();

    CHECK_IF(udp == NULL, return UDP_FAIL, "udp is null");
    CHECK_IF(udp->fd == NULL, return UDP_FAIL, "udp->fd is null");
    CHECK_IF(udp->is_init != 1, return UDP_FAIL, "udp is not initialized yet");
    CHECK_IF(buffer == NULL, return UDP_FAIL, "buffer is null");
    CHECK_IF(buffer_size <= 0, return UDP_FAIL, "buffer_size is %d", buffer_size);

    set_fs(get_ds());
    recvlen = _recvfrom(udp->fd, buffer, buffer_size, &remote_addr);
    set_fs(oldfs);
    chk = udp_to_udpaddr(remote_addr, remote);
    CHECK_IF(chk != UDP_OK, return -1, "udp_to_udpaddr failed");

    return recvlen;
}
