/**
 * Copyright (c) 2021 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */
#include <stdio.h>

#include "port_common.h"

#include "wizchip_conf.h"
#include "w6x00_spi.h"

#include "loopback.h"

#include "timer.h"

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
#define PLL_SYS_KHZ (133 * 1000)

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_TCP_SERVER 0
#define SOCKET_TCP_CLIENT 1
#define SOCKET_UDP 2
#define SOCKET_TCP_SERVER6 3
#define SOCKET_TCP_CLIENT6 4
#define SOCKET_UDP6 5
#define SOCKET_TCP_SERVER_DUAL 6
#define SOCKET_DHCP 7

/* Port */
#define PORT_TCP_SERVER 5000
#define PORT_TCP_CLIENT 5001
#define PORT_TCP_CLIENT_DEST    5002
#define PORT_UDP 5003

#define PORT_TCP_SERVER6 5004
#define PORT_TCP_CLIENT6 5005
#define PORT_TCP_CLIENT6_DEST 5006
#define PORT_UDP6 5007

#define PORT_TCP_SERVER_DUAL 5008

#define IPV4
#define IPV6

#ifdef IPV4
#define TCP_SERVER
#define TCP_CLIENT
#define UDP
#endif

#ifdef IPV6
#define TCP_SERVER6
#define TCP_CLIENT6
#define UDP6
#endif

#if defined IPV4 && defined IPV6
#define TCP_SERVER_DUAL
#endif

#define RETRY_CNT   10000

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
/* Network */
static wiz_NetInfo g_net_info =
    {
        .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address = 00:08:DC:12:34:56 -- MAC associated w/ this WizNet ip (next line)
        .ip = {192, 168, 0, 99},                     // IP address = 192.168.0.99 -- reserved on router!
        .sn = {255, 255, 255, 0},                    // Subnet Mask
        .gw = {192, 168, 0, 1},                     // Gateway
        // .dns = {208, 69, 40, 3},                        // DNS server = 208.69.40.3 -- obtained from router page
        .dns = {192, 168, 0, 1},                        // DNS server = 192.168.0.1 -- obtained from WizNet serial console
        .lla = {0xfe, 0x80, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x23, 0x40, 0xb9, 0x25,
                0x35, 0x07, 0xfa, 0x94},             // Link Local Address = fe80::2340:b925:3507:fa94%15
        .gua = {0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00},             // Global Unicast Address !! UNKNOWN !!
        .sn6 = {0x26, 0x07, 0xf5, 0x98,
                0xf4, 0x08, 0x2a, 0x00,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00},             // IPv6 Prefix = first half of Gateway IPv6 followed by zeroes.  
        .gw6 = {0x26, 0x07, 0xf5, 0x98,
                0xf4, 0x08, 0x2a, 0x00,
                0x76, 0xfe, 0xce, 0xff,
                0xfe, 0x37, 0x46, 0x10},             // Gateway IPv6 Address = 2607:F598:F408:2A00:76FE:CEFF:FE37:4610/64
                /*
        .dns6 = {0x26, 0x07, 0xf5, 0x98,
                0x00, 0x00, 0x11, 0x11,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x03},             // DNS6 server = 2607:F598:0:1::3 -- !!! WizNet console claims this is 0::0
                */
        .dns6 = {0x26, 0x07, 0xf5, 0x98,
                0xf4, 0x08, 0x2a, 0x00,
                0x76, 0xfe, 0xce, 0xff,
                0xfe, 0x37, 0x46, 0x10},            // DNS6 server -- ! Trying same as gateway for error check
        .ipmode = NETINFO_STATIC_ALL
};
/*
* Note -- dest IP is independet of WizNet device. It references the target computer/device. 
*/
uint8_t tcp_client_destip[] = { //192.168.0.223
    192, 168, 0, 223 // ip of target PC (this pc via Eth.)
};

uint8_t tcp_client_destip6[] = { //2607:f598:f408:2a00:1b34:6b42:f435:fb44
    0x26, 0x07, 0xf5, 0x98,
    0xf4, 0x08, 0x2a, 0x00, 
    0x1b, 0x34, 0x6b, 0x42,
    0xf4, 0x35, 0xfb, 0x44
};


uint16_t tcp_client_destport = PORT_TCP_CLIENT_DEST;

uint16_t tcp_client_destport6 = PORT_TCP_CLIENT6_DEST;

/* Loopback */
static uint8_t g_tcp_server_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};
static uint8_t g_tcp_client_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};
static uint8_t g_udp_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};
static uint8_t g_tcp_server6_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};
static uint8_t g_tcp_client6_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};
static uint8_t g_udp6_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};
static uint8_t g_tcp_server_dual_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};

/* Timer */
static volatile uint16_t g_msec_cnt = 0;

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void);

/* Timer */
static void repeating_timer_callback(void);

/**
 * ----------------------------------------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------------------------------------
 */
int main()
{
    /* Initialize */
    int retval = 0;
    uint8_t dhcp_retry = 0;
    uint8_t dns_retry = 0;

    set_clock_khz();

    stdio_init_all();

    sleep_ms(1000 * 3);

    printf("==========================================================\n");
    printf("Compiled @ %s, %s\n", __DATE__, __TIME__);
    printf("==========================================================\n");

    wizchip_spi_initialize();
    wizchip_cris_initialize();

    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    wizchip_1ms_timer_initialize(repeating_timer_callback);

    network_initialize(g_net_info);

    /* Get network information */
    print_network_information(g_net_info);

    /* Infinite loop */
    while (1)
    {
#ifdef TCP_SERVER
            /* TCP server loopback test */
            if ((retval = loopback_tcps(SOCKET_TCP_SERVER, g_tcp_server_buf, PORT_TCP_SERVER, AS_IPV4)) < 0)
            {
                printf(" loopback_tcps error : %d\n", retval);

                while (1)
                    ;
            }
#endif
#ifdef TCP_CLIENT
            /* TCP client loopback test */
            static uint32_t tcp_client_cnt = 0;
            if ((retval = loopback_tcpc(SOCKET_TCP_CLIENT, g_tcp_client_buf, tcp_client_destip, tcp_client_destport, AS_IPV4)) < 0)
            {
                printf(" loopback_tcpc error : %d\n", retval);

                while (1)
                    ;
            }
#endif
#ifdef UDP
            /* UDP loopback test */
            if ((retval = loopback_udps(SOCKET_UDP, g_udp_buf, PORT_UDP, AS_IPV4)) < 0)
            {
                printf(" loopback_udps error : %d\n", retval);

                while (1)
                    ;
            }
#endif
#ifdef TCP_SERVER6
            /* TCP server loopback test */
            if ((retval = loopback_tcps(SOCKET_TCP_SERVER6, g_tcp_server6_buf, PORT_TCP_SERVER6, AS_IPV6)) < 0)
            {
                printf(" loopback_tcps IPv6 error : %d\n", retval);

                while (1)
                    ;
            }
#endif
#ifdef TCP_CLIENT6
            /* TCP client loopback test */
            if ((retval = loopback_tcpc(SOCKET_TCP_CLIENT6, g_tcp_client6_buf, tcp_client_destip6, tcp_client_destport6, AS_IPV6)) < 0)
            {
                printf(" loopback_tcpc IPv6 error : %d\n", retval);

                while (1)
                    ;
            }
#endif
#ifdef UDP6
            /* UDP loopback test */
            if ((retval = loopback_udps(SOCKET_UDP6, g_udp6_buf, PORT_UDP6, AS_IPV6)) < 0)
            {
                printf(" loopback_udps IPv6 error : %d\n", retval);

                while (1)
                    ;
            }
#endif
#ifdef TCP_SERVER_DUAL
            /* TCP server dual loopback test */
            if ((retval = loopback_tcps(SOCKET_TCP_SERVER_DUAL, g_tcp_server_dual_buf, PORT_TCP_SERVER_DUAL, AS_IPDUAL)) < 0)
            {
                printf(" loopback_tcps IPv6 error : %d\n", retval);

                while (1)
                    ;
            }
#endif
    }
}

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void)
{
    // set a system clock frequency in khz
    set_sys_clock_khz(PLL_SYS_KHZ, true);

    // configure the specified clock
    clock_configure(
        clk_peri,
        0,                                                // No glitchless mux
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
        PLL_SYS_KHZ * 1000,                               // Input frequency
        PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
    );
}

/* Timer */
static void repeating_timer_callback(void)
{
    g_msec_cnt++;

    if (g_msec_cnt >= 1000 - 1)
    {
        g_msec_cnt = 0;
    }
}