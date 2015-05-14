#ifndef _KUDP_H_
#define _KUDP_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>             // for flip_open, filp_close
#include <linux/inet.h>           // for in4_pton
#include <net/sock.h>             // for IPPROTO_IP, SOCK_DGRAM, AF_INET
#include <linux/net.h>            // for sock_create, sock_alloc_file

#define UDP_PORT_ANY -1
#define UDP_OK (0)
#define UDP_FAIL (-1)

struct udp_addr
{
    char ipv4[INET_ADDRSTRLEN];
    int  port;
};

struct udp
{
    int local_port;
    struct socket* fd;
    int is_init;
};

int udp_send(struct udp* udp, struct udp_addr remote, void* data, int data_len);
int udp_recv(struct udp* udp, void* buffer, int buffer_size, struct udp_addr* remote);

int udp_init(struct udp* udp, char* local_ip, int local_port);
int udp_uninit(struct udp* udp);

int udp_to_udpaddr(struct sockaddr_in sock_addr, struct udp_addr* udp_addr);
int udp_to_sockaddr(struct udp_addr udp_addr, struct sockaddr_in* sock_addr);

#endif //_KUDP_H_
