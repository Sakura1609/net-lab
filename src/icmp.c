#include "net.h"
#include "icmp.h"
#include "ip.h"

/**
 * @brief 发送icmp响应
 * 
 * @param req_buf 收到的icmp请求包
 * @param src_ip 源ip地址
 */
static void icmp_resp(buf_t *req_buf, uint8_t *src_ip)
{
    // TO-DO
    buf_init(&txbuf, req_buf->len);
    icmp_hdr_t *ich = (icmp_hdr_t *)txbuf.data;
    icmp_hdr_t *src_ich = (icmp_hdr_t *)req_buf->data;
    ich->type = ICMP_TYPE_ECHO_REPLY;
    ich->code = ICMP_TYPE_ECHO_REPLY;
    ich->checksum16 = 0;
    ich->id16 = src_ich->id16;
    ich->seq16 = src_ich->seq16;
    memmove(txbuf.data + sizeof(icmp_hdr_t), req_buf->data + sizeof(icmp_hdr_t), req_buf->len - sizeof(icmp_hdr_t));
    ich->checksum16 = checksum16((uint16_t *)ich, req_buf->len);
    ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
    return;
}

/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 * @param src_ip 源ip地址
 */
void icmp_in(buf_t *buf, uint8_t *src_ip)
{
    // TO-DO
    if (buf->len < sizeof(icmp_hdr_t)) {
        printf("buffer too short\n");
        return;
    }
    icmp_hdr_t* ich = (icmp_hdr_t *)buf->data;
    if (ich->type == ICMP_TYPE_ECHO_REQUEST)
        icmp_resp(buf, src_ip);
    return;
    
}

/**
 * @brief 发送icmp不可达
 * 
 * @param recv_buf 收到的ip数据包
 * @param src_ip 源ip地址
 * @param code icmp code，协议不可达或端口不可达
 */
void icmp_unreachable(buf_t *recv_buf, uint8_t *src_ip, icmp_code_t code)
{
    // TO-DO
    buf_init(&txbuf, sizeof(icmp_code_t) + sizeof(ip_hdr_t) + 8);
    icmp_hdr_t *ich = (icmp_hdr_t *)txbuf.data;
    ip_hdr_t *ih = (ip_hdr_t *)recv_buf->data;
    ich->checksum16 = 0;
    ich->type = ICMP_TYPE_UNREACH;
    ich->code = code;
    ich->id16 = 0;
    ich->seq16 = 0;
    memmove(txbuf.data + sizeof(icmp_code_t), recv_buf->data, sizeof(ip_hdr_t) + 8);
    ich->checksum16 = checksum16((uint16_t *)ich, sizeof(icmp_code_t) + sizeof(ip_hdr_t) + 8);
    ip_out(&txbuf, src_ip, ih->protocol);
    return;
}

/**
 * @brief 初始化icmp协议
 * 
 */
void icmp_init(){
    net_add_protocol(NET_PROTOCOL_ICMP, icmp_in);
}