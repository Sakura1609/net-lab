#include "udp.h"
#include "ip.h"
#include "icmp.h"

/**
 * @brief udp处理程序表
 * 
 */
map_t udp_table;
extern size_t id;

/**
 * @brief udp伪校验和计算
 * 
 * @param buf 要计算的包
 * @param src_ip 源ip地址
 * @param dst_ip 目的ip地址
 * @return uint16_t 伪校验和
 */
static uint16_t udp_checksum(buf_t *buf, uint8_t *src_ip, uint8_t *dst_ip)
{
    // TO-DO
    buf_add_header(buf, sizeof(udp_peso_hdr_t));
    ip_hdr_t ip_head;
    memmove(&ip_head, buf->data, sizeof(ip_hdr_t));
    udp_peso_hdr_t *uph = (udp_peso_hdr_t *)buf->data;
    memmove(uph->dst_ip, dst_ip, NET_IP_LEN);
    memmove(uph->src_ip, src_ip, NET_IP_LEN);
    uph->protocol = ip_head.protocol;
    uph->total_len16 = buf->len - sizeof(udp_peso_hdr_t);

    size_t len = buf->len;
    if (len % 2) 
        buf_add_padding(buf, 1);
    
    uint16_t checksum = checksum16((uint16_t *)buf->data, buf->len);
    
    if (len % 2) 
        buf_remove_padding(buf, 1);

    memmove(buf->data, &ip_head, sizeof(ip_hdr_t));
    buf_remove_header(buf, sizeof(udp_peso_hdr_t));

    return checksum;
}

/**
 * @brief 处理一个收到的udp数据包
 * 
 * @param buf 要处理的包
 * @param src_ip 源ip地址
 */
void udp_in(buf_t *buf, uint8_t *src_ip)
{
    // TO-DO
    if (buf->len < sizeof(udp_hdr_t)) {
        printf("buffer too short\n");
        return;
    }

    udp_hdr_t *uh = (udp_hdr_t *)buf->data;
    uint16_t origin_checksum16 = uh->checksum16;
    uh->checksum16 = 0;
    uh->checksum16 = udp_checksum(buf, src_ip, net_if_ip);
    if (origin_checksum16 != uh->checksum16) {
        printf("udp_in checksum failed\n");
        return;
    }

    udp_handler_t *handler = (udp_handler_t *)map_get(&udp_table, &uh->dst_port16);

    if (handler) {
        buf_remove_header(buf, sizeof(udp_hdr_t));
        (*handler)(buf->data, uh->total_len16, src_ip, uh->src_port16);
        return;
    }

    buf_add_header(buf, sizeof(ip_hdr_t));
    // ip_hdr_t *iph = (ip_hdr_t *)buf->data;
    // iph->version = IP_VERSION_4;
    // iph->hdr_len = 5;
    // iph->tos = 0x0;
    // iph->total_len16 = swap16(buf->len);
    // id++;
    // iph->id16 = swap16(id);
    // iph->flags_fragment16 = swap16(0 << 13 | 0);
    // iph->hdr_checksum16 = 0x0;
    // iph->protocol = protocol;
    // iph->ttl = IP_DEFALUT_TTL;
    // memmove(iph->dst_ip, src_ip, NET_IP_LEN);
    // memmove(iph->src_ip, net_if_ip, NET_IP_LEN);
    // iph->hdr_checksum16 = checksum16((uint16_t *)buf->data, sizeof(ip_hdr_t));
    icmp_unreachable(buf, src_ip, ICMP_CODE_PORT_UNREACH);
}

/**
 * @brief 处理一个要发送的数据包
 * 
 * @param buf 要处理的包
 * @param src_port 源端口号
 * @param dst_ip 目的ip地址
 * @param dst_port 目的端口号
 */
void udp_out(buf_t *buf, uint16_t src_port, uint8_t *dst_ip, uint16_t dst_port)
{
    // TO-DO
    // ip_hdr_t *iph = (ip_hdr_t *)buf->data;
    // protocol = iph->protocol;
    
    buf_add_header(buf, sizeof(udp_hdr_t));
    udp_hdr_t *uh = (udp_hdr_t *)buf->data;
    uh->dst_port16 = dst_port;
    uh->src_port16 = src_port;
    uh->checksum16 = 0;
    uh->total_len16 = buf->len;

    uh->checksum16 = udp_checksum(buf, net_if_ip, dst_ip);
    ip_out(buf, dst_ip, NET_PROTOCOL_TCP);
}

/**
 * @brief 初始化udp协议
 * 
 */
void udp_init()
{
    map_init(&udp_table, sizeof(uint16_t), sizeof(udp_handler_t), 0, 0, NULL);
    net_add_protocol(NET_PROTOCOL_UDP, udp_in);
}

/**
 * @brief 打开一个udp端口并注册处理程序
 * 
 * @param port 端口号
 * @param handler 处理程序
 * @return int 成功为0，失败为-1
 */
int udp_open(uint16_t port, udp_handler_t handler)
{
    return map_set(&udp_table, &port, &handler);
}

/**
 * @brief 关闭一个udp端口
 * 
 * @param port 端口号
 */
void udp_close(uint16_t port)
{
    map_delete(&udp_table, &port);
}

/**
 * @brief 发送一个udp包
 * 
 * @param data 要发送的数据
 * @param len 数据长度
 * @param src_port 源端口号
 * @param dst_ip 目的ip地址
 * @param dst_port 目的端口号
 */
void udp_send(uint8_t *data, uint16_t len, uint16_t src_port, uint8_t *dst_ip, uint16_t dst_port)
{
    buf_init(&txbuf, len);
    memcpy(txbuf.data, data, len);
    udp_out(&txbuf, src_port, dst_ip, dst_port);
}