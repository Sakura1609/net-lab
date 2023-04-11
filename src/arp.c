#include <string.h>
#include <stdio.h>
#include "net.h"
#include "arp.h"
#include "ethernet.h"
/**
 * @brief 初始的arp包
 *
 */
static const arp_pkt_t arp_init_pkt = {
    .hw_type16 = constswap16(ARP_HW_ETHER),
    .pro_type16 = constswap16(NET_PROTOCOL_IP),
    .hw_len = NET_MAC_LEN,
    .pro_len = NET_IP_LEN,
    .sender_ip = NET_IF_IP,
    .sender_mac = NET_IF_MAC,
    .target_mac = {0}};

/**
 * @brief arp地址转换表，<ip,mac>的容器
 *
 */
map_t arp_table;

/**
 * @brief arp buffer，<ip,buf_t>的容器
 *
 */
map_t arp_buf;

/**
 * @brief 打印一条arp表项
 *
 * @param ip 表项的ip地址
 * @param mac 表项的mac地址
 * @param timestamp 表项的更新时间
 */
void arp_entry_print(void *ip, void *mac, time_t *timestamp)
{
    printf("%s | %s | %s\n", iptos(ip), mactos(mac), timetos(*timestamp));
}

/**
 * @brief 打印整个arp表
 *
 */
void arp_print()
{
    printf("===ARP TABLE BEGIN===\n");
    map_foreach(&arp_table, arp_entry_print);
    printf("===ARP TABLE  END ===\n");
}

/**
 * @brief 发送一个arp请求
 *
 * @param target_ip 想要知道的目标的ip地址
 */
void arp_req(uint8_t *target_ip)
{
    // TO-DO
    buf_init(&txbuf, ETHERNET_MAX_TRANSPORT_UNIT + sizeof(arp_pkt_t));
    if (buf_add_header(&txbuf, sizeof(arp_pkt_t)) == -1)
    {
        printf("failed to add header\n");
        return;
    }
    arp_pkt_t *pkt = (arp_pkt_t *)txbuf.data;
    memcpy(pkt, &arp_init_pkt, sizeof(arp_pkt_t));
    memmove(pkt->target_ip, target_ip, NET_IP_LEN);
    pkt->opcode16 = constswap16(0x1);
    ethernet_out(&txbuf, ether_broadcast_mac, NET_PROTOCOL_IP);
}

/**
 * @brief 发送一个arp响应
 *
 * @param target_ip 目标ip地址
 * @param target_mac 目标mac地址
 */
void arp_resp(uint8_t *target_ip, uint8_t *target_mac)
{
    // TO-DO
    buf_init(&txbuf, ETHERNET_MAX_TRANSPORT_UNIT + sizeof(arp_pkt_t));
    if (buf_add_header(&txbuf, sizeof(arp_pkt_t)) == -1)
    {
        printf("failed to add header\n");
        return;
    }
    arp_pkt_t *pkt = (arp_pkt_t *)txbuf.data;
    memcpy(pkt, &arp_init_pkt, sizeof(arp_pkt_t));
    memmove(pkt->target_mac, target_mac, NET_MAC_LEN);
    memmove(pkt->target_ip, target_ip, NET_IP_LEN);
    pkt->opcode16 = constswap16(0x2);
    ethernet_out(&txbuf, target_mac, NET_PROTOCOL_IP);
}

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void arp_in(buf_t *buf, uint8_t *src_mac)
{
    // TO-DO
    buf_remove_header(buf, sizeof(ether_hdr_t));
    if (buf->len < sizeof(arp_pkt_t))
    {
        printf("invalid pkg length, abort!\n");
        return;
    }
    arp_pkt_t *pkt = (arp_pkt_t *)buf;

    // uint16_t hw_type16 = pkt->hw_type16;
    // uint16_t pro_type16 = pkt->pro_type16;
    // uint8_t hw_len = pkt->hw_len;
    // uint8_t pro_len = pkt->pro_len;
    // uint16_t opcode16 = pkt->opcode16;

    // if (hw_type16 != constswap16(ARP_HW_ETHER) || pro_type16 != constswap16(NET_PROTOCOL_IP) ||
    //     hw_len != NET_MAC_LEN || pro_len != NET_IP_LEN || (opcode16 != constswap16(0x1) && opcode16 != constswap16(0x2)))
    // {
    //     printf("invalid pkg header, abort!\n");
    //     return;
    // }
    uint8_t *ip = (uint8_t *)malloc(sizeof(NET_IP_LEN));
    memcpy(ip, pkt->target_ip, sizeof(NET_IP_LEN));
    map_set(&arp_table, ip, src_mac);

    buf_t *cache;
    if (!(cache = (buf_t *)map_get(&arp_buf, ip)))
    {
        ethernet_out(cache, src_mac, NET_PROTOCOL_ARP);
        map_delete(&arp_buf, ip);
    }

    if (pkt->opcode16 == constswap16(0x1) && pkt->target_ip == net_if_ip)
    {
        arp_resp(pkt->target_ip, src_mac);
        return;
    }
}

/**
 * @brief 处理一个要发送的数据包
 *
 * @param buf 要处理的数据包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void arp_out(buf_t *buf, uint8_t *ip)
{
    // TO-DO
    uint8_t *dst_mac;
    if ((dst_mac = (uint8_t *)map_get(&arp_table, ip)) != NULL)
    {
        ethernet_out(buf, dst_mac, NET_PROTOCOL_IP);
        return;
    }
    buf_t *cache;
    if (!(cache = (buf_t *)map_get(&arp_buf, ip)))
    {
        map_set(&arp_buf, ip, buf);
        arp_req(ip);
        return;
    }

    // TODO: extend buffer cache size in order not to loss buf
    //  do nothing if cache valid
    printf("loss pkg\n");
    return;
}

/**
 * @brief 初始化arp协议
 *
 */
void arp_init()
{
    map_init(&arp_table, NET_IP_LEN, NET_MAC_LEN, 0, ARP_TIMEOUT_SEC, NULL);
    map_init(&arp_buf, NET_IP_LEN, sizeof(buf_t), 0, ARP_MIN_INTERVAL, buf_copy);
    net_add_protocol(NET_PROTOCOL_ARP, arp_in);
    arp_req(net_if_ip);
}