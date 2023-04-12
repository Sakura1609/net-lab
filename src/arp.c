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

    // 生成新的arp包，广播，仅用于寻找目标ip，因此长度只有一个arp_pkt_t结构体大小
    buf_init(&txbuf, sizeof(arp_pkt_t));
    arp_pkt_t *pkt = (arp_pkt_t *)txbuf.data;
    *pkt = arp_init_pkt;
    memmove(pkt->target_ip, target_ip, NET_IP_LEN);
    pkt->opcode16 = constswap16(ARP_REQUEST);
    ethernet_out(&txbuf, ether_broadcast_mac, NET_PROTOCOL_ARP);
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

    // 生成新的arp包，发往目标，告知目标自己的mac与ip，长度只有一个arp_pkt_t大小
    buf_init(&txbuf, sizeof(arp_pkt_t));
    arp_pkt_t *pkt = (arp_pkt_t *)txbuf.data;
    *pkt = arp_init_pkt;
    memmove(pkt->target_mac, target_mac, NET_MAC_LEN);
    memmove(pkt->target_ip, target_ip, NET_IP_LEN);
    pkt->opcode16 = constswap16(ARP_REPLY);
    ethernet_out(&txbuf, target_mac, NET_PROTOCOL_ARP);
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

    // 判断包长度是否合法，至少为一个协议头大小
    if (buf->len < sizeof(arp_pkt_t))
    {
        printf("invalid pkg length, abort!\n");
        return;
    }
    arp_pkt_t *pkt = (arp_pkt_t *)buf->data;

    uint16_t hw_type16 = pkt->hw_type16;
    uint16_t pro_type16 = pkt->pro_type16;
    uint8_t hw_len = pkt->hw_len;
    uint8_t pro_len = pkt->pro_len;
    uint16_t opcode16 = pkt->opcode16;

    // 判断协议是否合法
    if (hw_type16 != constswap16(ARP_HW_ETHER) || pro_type16 != constswap16(NET_PROTOCOL_IP) ||
        hw_len != NET_MAC_LEN || pro_len != NET_IP_LEN || 
        (opcode16 != constswap16(ARP_REPLY) && opcode16 != constswap16(ARP_REQUEST)))
    {
        printf("invalid pkg header, abort!\n");
        return;
    }

    // 填写arp ip表，记录ip与mac对应信息
    map_set(&arp_table, pkt->sender_ip, src_mac);
    
    // 查找arp 数据包表，如果存在缓存则直接发送，并删除。
    buf_t *cache = (buf_t *)map_get(&arp_buf, pkt->sender_ip);
        if (cache) {
            ethernet_out(cache, src_mac, NET_PROTOCOL_IP);
            map_delete(&arp_buf, pkt->sender_ip);
            return;
        }

    // 否则判断是否为arp请求报文，进行发送
    if (pkt->opcode16 == constswap16(ARP_REQUEST) && !memcmp(pkt->target_ip, net_if_ip, NET_IP_LEN))
    {
        arp_resp(pkt->sender_ip, src_mac);
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

    // 查表，直接发送
    if ((dst_mac = (uint8_t *)map_get(&arp_table, ip)) != NULL)
    {
        ethernet_out(buf, dst_mac, NET_PROTOCOL_IP);
        return;
    }

    // 不在arp表中，查看cache中是否有数据。
    // 若cache中无数据，则说明是新ip，发送获取ip的请求
    buf_t *cache = (buf_t *)map_get(&arp_buf, ip);

    
    if (!cache)
        arp_req(ip);
    
    // 使用cache容量为1， 则只有cache中无内容时才将数据填入cache
    map_set(&arp_buf, ip, buf);
}

/**
 * @brief 初始化arp协议
 *
 */
void arp_init()
{
    map_init(&arp_table, NET_IP_LEN, NET_MAC_LEN, 0, ARP_TIMEOUT_SEC, NULL, 1);
    map_init(&arp_buf, NET_IP_LEN, sizeof(buf_t), 0, ARP_MIN_INTERVAL, buf_copy, 1);
    net_add_protocol(NET_PROTOCOL_ARP, arp_in);
    arp_req(net_if_ip);
}