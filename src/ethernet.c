#include "ethernet.h"
#include "utils.h"
#include "driver.h"
#include "arp.h"
#include "ip.h"

/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 */
void ethernet_in(buf_t *buf)
{
    // TO-DO
    if (buf->len < sizeof(ether_hdr_t)) {
        printf("buffer too short\n");
        return;
    }
    ether_hdr_t *hdr = (ether_hdr_t *)buf->data;
    uint16_t proto = swap16(hdr->protocol16);
    uint8_t mac[NET_MAC_LEN];
    memmove(mac, hdr->src, NET_MAC_LEN);
    if (buf_remove_header(buf, sizeof(ether_hdr_t)) == -1) {
        printf("failed to remove header");
        return;
    }
    if (net_in(buf, proto, mac) == -1) {
        // printf("failed to give upper buffer");
        return;
    }
}
/**
 * @brief 处理一个要发送的数据包
 * 
 * @param buf 要处理的数据包
 * @param mac 目标MAC地址
 * @param protocol 上层协议
 */
void ethernet_out(buf_t *buf, const uint8_t *mac, net_protocol_t protocol)
{
    // TO-DO
    if (buf->len < 46) {
        if (buf_add_padding(buf, 46 - buf->len) == -1) {
            printf("failed to add pad\n");
            return ;
        }
    }
    if (buf_add_header(buf, sizeof(ether_hdr_t)) == -1) {
        printf("failed to add header\n");
        return;
    }
    ether_hdr_t *hdr = (ether_hdr_t *)buf->data;
    memmove(hdr->dst, mac, NET_MAC_LEN);
    memmove(hdr->src, net_if_mac, NET_MAC_LEN);
    hdr->protocol16 = swap16(protocol);
    if (driver_send(buf) == -1) {
        printf("failed to send buffer\n");
        return;
    }
}
/**
 * @brief 初始化以太网协议
 * 
 */
void ethernet_init()
{
    buf_init(&rxbuf, ETHERNET_MAX_TRANSPORT_UNIT + sizeof(ether_hdr_t));
}

/**
 * @brief 一次以太网轮询
 * 
 */
void ethernet_poll()
{
    if (driver_recv(&rxbuf) > 0)
        ethernet_in(&rxbuf);
}
