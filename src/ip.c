#include "net.h"
#include "ip.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"

size_t id = 0;
/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void ip_in(buf_t *buf, uint8_t *src_mac)
{
    // TO-DO
    // check buf len
    if (buf->len < sizeof(ip_hdr_t)) {
        printf("buffer too short\n");
        return;
    }
    // check head
    ip_hdr_t *iph = (ip_hdr_t *)buf->data;
    uint8_t version = swap16(iph->version);
    uint8_t hdr_len = swap16(iph->hdr_len);
    if (version != IP_VERSION_4 || hdr_len != IP_HDR_LEN_PER_BYTE) {
        printf("invalid pkg header, abort\n");
        return;
    }
    uint16_t origin_check = iph->hdr_checksum16;
    iph->hdr_checksum16 = 0;
    uint16_t res = checksum16(buf->data, sizeof(ip_hdr_t));
    if (res != origin_check) {
        printf("checksum failed\n");
        return;
    }
    iph->hdr_checksum16 = origin_check;
    
    if (iph->dst_ip != net_if_ip) {
        return;
    }

    if (buf->len > iph->total_len16)
        buf_remove_padding(buf->data, buf->len - iph->total_len16);
    uint8_t protocal = swap16(iph->protocol);
    uint8_t src_ip[NET_IP_LEN];
    memmove(src_ip, iph->src_ip, NET_IP_LEN);
    buf_remove_header(buf->data, sizeof(ip_hdr_t));
    net_in(buf, protocal, src_ip);
}

/**
 * @brief 处理一个要发送的ip分片
 * 
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf)
{
    // TO-DO
    buf_add_header(buf, sizeof(ip_hdr_t));
    ip_hdr_t *iph = (ip_hdr_t *)buf->data;
    iph->version = constswap16(IP_VERSION_4);
    iph->hdr_len = constswap16(IP_HDR_LEN_PER_BYTE);
    iph->tos = 0x0;
    // iph->total_len16 = 
    arp_out(buf, ip);
}

/**
 * @brief 处理一个要发送的ip数据包
 * 
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    // TO-DO
    size_t len = buf->len;
    size_t max_data =  ETHERNET_MAX_TRANSPORT_UNIT - sizeof(ip_hdr_t);
    uint16_t offset = 0;
    id++;
    if (len <= max_data) {
        ip_fragment_out(buf, ip, protocol, id, offset, 0);
        return;
    }
    uint16_t add = max_data / IP_HDR_OFFSET_PER_BYTE;
    while(1) {
        buf_init(&txbuf, ETHERNET_MAX_TRANSPORT_UNIT);
        if (len > max_data) {
            len -= max_data;
            offset +=  add;
            ip_fragment_out(buf, ip, protocol, id, offset, 1);
            continue;
        }
        offset += add;
        ip_fragment_out(buf, ip, protocol, id, offset, 0);
        return;
    }
}

/**
 * @brief 初始化ip协议
 * 
 */
void ip_init()
{
    net_add_protocol(NET_PROTOCOL_IP, ip_in);
}