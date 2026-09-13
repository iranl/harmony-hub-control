#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef SO_BINDTODEVICE
#define SO_BINDTODEVICE 25
#endif

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DHCP_MAGIC 0x63825363U

static unsigned char *opt_find(unsigned char *opts, int len, int code) {
    int i = 0;
    while (i < len) {
        int c = opts[i++];
        int l;
        if (c == 0) continue;
        if (c == 255) break;
        if (i >= len) break;
        l = opts[i++];
        if (i + l > len) break;
        if (c == code) return &opts[i - 2];
        i += l;
    }
    return NULL;
}

static void add_opt(unsigned char **p, int code, const void *data, int len) {
    *(*p)++ = (unsigned char)code;
    *(*p)++ = (unsigned char)len;
    memcpy(*p, data, len);
    *p += len;
}

static unsigned int ip_to_u32(const char *s) {
    struct in_addr a;
    if (inet_aton(s, &a) == 0) return 0;
    return ntohl(a.s_addr);
}

static void u32_to_ip(unsigned int v, struct in_addr *a) {
    a->s_addr = htonl(v);
}

static unsigned int pick_ip(unsigned int start, unsigned int end, const unsigned char *mac) {
    unsigned int span = end >= start ? end - start + 1 : 1;
    unsigned int n = mac[3] ^ mac[4] ^ mac[5];
    return start + (n % span);
}

int main(int argc, char **argv) {
    const char *iface;
    unsigned int server_ip, start_ip, end_ip;
    int fd, one = 1;
    struct sockaddr_in addr;

    if (argc < 5) {
        fprintf(stderr, "usage: %s <iface> <server-ip> <lease-start> <lease-end>\n", argv[0]);
        return 2;
    }
    iface = argv[1];
    server_ip = ip_to_u32(argv[2]);
    start_ip = ip_to_u32(argv[3]);
    end_ip = ip_to_u32(argv[4]);
    if (!server_ip || !start_ip || !end_ip) {
        fprintf(stderr, "invalid ip argument\n");
        return 2;
    }

    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        perror("socket");
        return 1;
    }
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
    setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, iface, strlen(iface));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DHCP_SERVER_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(fd);
        return 1;
    }
    fprintf(stderr, "codex_dhcpd listening on %s\n", iface);

    while (1) {
        unsigned char req[1500];
        unsigned char resp[1500];
        unsigned char *opts;
        unsigned char *msgopt;
        unsigned char *p;
        struct sockaddr_in dst;
        struct in_addr a;
        ssize_t n = recv(fd, req, sizeof(req), 0);
        int msgtype;
        unsigned int yiaddr;
        unsigned int lease = htonl(3600);
        unsigned int mask = htonl(0xffffff00U);
        unsigned int srv = htonl(server_ip);

        if (n < 244) continue;
        { unsigned int magic; memcpy(&magic, &req[236], 4); if (ntohl(magic) != DHCP_MAGIC) continue; }
        opts = &req[240];
        msgopt = opt_find(opts, (int)n - 240, 53);
        if (!msgopt || msgopt[1] < 1) continue;
        msgtype = msgopt[2];
        if (msgtype != 1 && msgtype != 3) continue;

        memset(resp, 0, sizeof(resp));
        memcpy(resp, req, 236);
        resp[0] = 2;
        yiaddr = pick_ip(start_ip, end_ip, &req[28]);
        u32_to_ip(yiaddr, &a);
        memcpy(&resp[16], &a.s_addr, 4);
        { unsigned int magic_out = htonl(DHCP_MAGIC); memcpy(&resp[236], &magic_out, 4); }
        p = &resp[240];
        {
            unsigned char mt = msgtype == 1 ? 2 : 5;
            add_opt(&p, 53, &mt, 1);
        }
        add_opt(&p, 54, &srv, 4);
        add_opt(&p, 51, &lease, 4);
        add_opt(&p, 1, &mask, 4);
        add_opt(&p, 3, &srv, 4);
        add_opt(&p, 6, &srv, 4);
        *p++ = 255;

        memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        dst.sin_port = htons(DHCP_CLIENT_PORT);
        dst.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        sendto(fd, resp, (size_t)(p - resp), 0, (struct sockaddr *)&dst, sizeof(dst));
        fprintf(stderr, "%s %s to %02x:%02x:%02x:%02x:%02x:%02x\n",
            msgtype == 1 ? "offer" : "ack", inet_ntoa(a),
            req[28], req[29], req[30], req[31], req[32], req[33]);
    }
}
