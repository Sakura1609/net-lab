#include "http.h"
#include "tcp.h"
#include "net.h"
#include "assert.h"
#include <stdio.h>

#define TCP_FIFO_SIZE 40

typedef struct http_fifo {
    tcp_connect_t* buffer[TCP_FIFO_SIZE];
    uint8_t front, tail, count;
} http_fifo_t;

static http_fifo_t http_fifo_v;

static void http_fifo_init(http_fifo_t* fifo) {
    fifo->count = 0;
    fifo->front = 0;
    fifo->tail = 0;
}

static int http_fifo_in(http_fifo_t* fifo, tcp_connect_t* tcp) {
    if (fifo->count >= TCP_FIFO_SIZE) {
        return -1;
    }
    fifo->buffer[fifo->front] = tcp;
    fifo->front++;
    if (fifo->front >= TCP_FIFO_SIZE) {
        fifo->front = 0;
    }
    fifo->count++;
    return 0;
}

static tcp_connect_t* http_fifo_out(http_fifo_t* fifo) {
    if (fifo->count == 0) {
        return NULL;
    }
    tcp_connect_t* tcp = fifo->buffer[fifo->tail];
    fifo->tail++;
    if (fifo->tail >= TCP_FIFO_SIZE) {
        fifo->tail = 0;
    }
    fifo->count--;
    return tcp;
}

static size_t get_line(tcp_connect_t* tcp, char* buf, size_t size) {
    size_t i = 0;
    while (i < size) {
        char c;
        if (tcp_connect_read(tcp, (uint8_t*)&c, 1) > 0) {
            if (c == '\n') {
                break;
            }
            if (c != '\n' && c != '\r') {
                buf[i] = c;
                i++;
            }
            printf("tcp rx_buf len: %d\n", tcp->rx_buf->len);
        }
        net_poll();
    }
    buf[i] = '\0';
    return i;
}

static size_t http_send(tcp_connect_t* tcp, const char* buf, size_t size) {
    size_t send = 0;
    while (send < size) {
        send += tcp_connect_write(tcp, (const uint8_t*)buf + send, size - send);
        net_poll();
    }
    return send;
}

static void close_http(tcp_connect_t* tcp) {
    tcp_connect_close(tcp);
    printf("http closed.\n");
}



static void send_file(tcp_connect_t* tcp, const char* url) {
    FILE* file;
    uint32_t size = 0;
    // const char* content_type = "text/html";
    char file_path[255];
    char tx_buffer[1024];

    /*
    解析url路径，查看是否是查看XHTTP_DOC_DIR目录下的文件
    如果不是，则发送404 NOT FOUND
    如果是，则用HTTP/1.0协议发送

    注意，本实验的WEB服务器网页存放在XHTTP_DOC_DIR目录中
    */
   
   const char notfind[] = {"404 NOT FOUND\0"};
   if (url[size] != ' ')    size++;
   memmove(file_path, url, size);
   if (memcmp(file_path, XHTTP_DOC_DIR, sizeof(XHTTP_DOC_DIR) == 0)) {
        file = fopen(file_path, "rb");
        // 若size>1024应该如何处理？
        size = fseek(file, 0, SEEK_END);
        fread(tx_buffer, sizeof(char), size, file);
        http_send(tcp, tx_buffer, size);
        return;
    }
    http_send(tcp, notfind, sizeof(notfind));
}

static void http_handler(tcp_connect_t* tcp, connect_state_t state) {
    if (state == TCP_CONN_CONNECTED) {
        http_fifo_in(&http_fifo_v, tcp);
        printf("http conntected.\n");
    } else if (state == TCP_CONN_DATA_RECV) {
    } else if (state == TCP_CONN_CLOSED) {
        printf("http closed.\n");
    } 
    // else {
    //     assert(0);
    // }
}


// 在端口上创建服务器。

int http_server_open(uint16_t port) {
    if (!tcp_open(port, http_handler)) {
        return -1;
    }
    http_fifo_init(&http_fifo_v);
    return 0;
}

// 从FIFO取出请求并处理。新的HTTP请求时会发送到FIFO中等待处理。

void http_server_run(void) {
    tcp_connect_t* tcp;
    char url_path[255];
    char rx_buffer[1024];

    while ((tcp = http_fifo_out(&http_fifo_v)) != NULL) {
        char* c = rx_buffer;
        


        /*
        1、调用get_line从rx_buffer中获取一行数据，如果没有数据，则调用close_http关闭tcp，并继续循环
        */

        get_line(tcp, c, 255);
        printf("success get line\n");
        if (*c == '\0') {
            printf("get null buf\n");
            close_http(tcp);
            continue;
        }   

        /*
        2、检查是否有GET请求，如果没有，则调用close_http关闭tcp，并继续循环
        */
        if (memcmp(c, "GET", 3)) {
            printf("No GET\n");
            close_http(tcp);
            continue;
        }


        /*
        3、解析GET请求的路径，注意跳过空格，找到GET请求的文件，调用send_file发送文件
        */
        printf("GET!\n");
        c += 3;
        while (*c == ' ')    c++;
        memmove(url_path, c, sizeof(url_path));
        send_file(tcp, url_path);

        /*
        4、调用close_http关掉连接
        */

        close_http(tcp);

        printf("!! final close\n");
    }
}
