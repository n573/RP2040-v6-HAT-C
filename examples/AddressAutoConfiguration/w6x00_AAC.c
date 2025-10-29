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
#include "AddressAutoConfig.h"
#include "dhcpv4.h"

#include "timer.h"

#include "dns.h"

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
#define SOCKET_DHCP 6
#define SOCKET_DNS 7

/* Port */
#define PORT_TCP_SERVER 5000
#define PORT_TCP_CLIENT 5001
#define PORT_TCP_CLIENT_DEST    5002
#define PORT_UDP 5003

#define PORT_TCP_SERVER6 5004
#define PORT_TCP_CLIENT6 5005
#define PORT_TCP_CLIENT6_DEST 5006
#define PORT_UDP6 5007

#define IPV4
#define IPV6

#ifdef IPV4
#define TCP_SERVER
#define TCP_CLIENT
#define UDP
#define DHCP_RETRY_COUNT 5
#define DHCP4
#define DNS
#endif

#ifdef IPV6
#define TCP_SERVER6
#define TCP_CLIENT6
#define UDP6
#define ADDRS_AUTO_CONFIG
#endif

#define DNS_RETRY_COUNT 5

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

static uint8_t g_ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
}; // common buffer

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

/* DHCP */
static uint8_t g_dhcp_get_ip_flag = 0;

/* DNS */
static uint8_t g_dns_target_domain[] = "www.wiznet.io";
static uint8_t g_dns_target_ip[4] = {
    0,
};
static uint8_t g_dns6_target_domain[] = "www.google.com";
static uint8_t g_dns6_target_ip[16] = {
    0,
};
static uint8_t g_dns_get_ip_flag = 0;
uint8_t IP_TYPE;

/* Timer */
static volatile uint16_t g_msec_cnt = 0;

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void);

/* DHCP */
static void wizchip_dhcp4_init(void);
static void wizchip_dhcp4_assign(void);
static void wizchip_dhcp4_conflict(void);

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

    #ifdef DHCP4
    g_net_info.ipmode = NETINFO_DHCP_V4;
    #else
    g_net_info.ipmode = NETINFO_STATIC_V4;
    #endif

    #ifdef IPV6
    #ifdef ADDRS_AUTO_CONFIG
    g_net_info.ipmode |= NETINFO_SLAAC_V6;
    #else
    g_net_info.ipmode |= NETINFO_STATIC_V6;
    #endif
    #endif

    network_initialize(g_net_info);

    /* Get network information */
    print_network_information(g_net_info);

    #ifdef ADDRS_AUTO_CONFIG
    if(g_net_info.ipmode & NETINFO_SLAAC_V6)
    {
        if(1 != AddressAutoConfig_Init(&g_net_info))
        {
            printf("Address Auto Config failed\n");
        }
    }
    #endif

    #ifdef DHCP4
    if (g_net_info.ipmode & NETINFO_DHCP_V4) // DHCP
    {
        wizchip_dhcp4_init();
    }
    #else
    g_dhcp_get_ip_flag = 1;
    #endif

    #ifdef DNS
    DNS_init(g_ethernet_buf);
    #endif

    /* Infinite loop */
    while (1)
    {
        #ifdef DHCP4
        /* Assigned IP through DHCP */
        if (g_net_info.ipmode & NETINFO_DHCP_V4)
        {
            retval = DHCPv4_run();

            if (retval == DHCP_IP_LEASED)
            {
                if (g_dhcp_get_ip_flag == 0)
                {
                    printf(" DHCP success\n");

                    g_dhcp_get_ip_flag = 1;
                }
            }
            else if (retval == DHCP_FAILED)
            {
                g_dhcp_get_ip_flag = 0;
                dhcp_retry++;

                if (dhcp_retry <= DHCP_RETRY_COUNT)
                {
                    printf(" DHCP timeout occurred and retry %d\n", dhcp_retry);
                }
            }

            if (dhcp_retry > DHCP_RETRY_COUNT)
            {
                printf(" DHCP failed\n");

                DHCPv4_stop();

                while (1)
                    ;
            }

            if(g_dhcp_get_ip_flag == 0)
            {
                wizchip_delay_ms(1000); // wait for 1 second
            }
        }
        #endif

        if(g_dhcp_get_ip_flag == 1)
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

            #ifdef DNS
            /* Get IP through DNS */
            if (g_dns_get_ip_flag == 0)
            {
                IP_TYPE = 0x1;
                if (DNS_run(SOCKET_DNS, g_net_info.dns, g_dns_target_domain, g_dns_target_ip, AS_IPV4) > 0)
                {
                    printf(" DNS success\n");
                    printf(" Target domain : %s\n", g_dns_target_domain);
                    printf(" IP of target domain : %d.%d.%d.%d\n", g_dns_target_ip[0], g_dns_target_ip[1], g_dns_target_ip[2], g_dns_target_ip[3]);

                    IP_TYPE = 0x1c;
                    if (DNS_run(SOCKET_DNS, g_net_info.dns, g_dns6_target_domain, g_dns6_target_ip, AS_IPV4) > 0)
                    {
                        printf(" DNS success\n");
                        printf(" Target domain : %s\n", g_dns6_target_domain);
                        printf(" IP of target domain : %.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x\n"
                        , g_dns6_target_ip[0], g_dns6_target_ip[1], g_dns6_target_ip[2], g_dns6_target_ip[3]
                        , g_dns6_target_ip[4], g_dns6_target_ip[5], g_dns6_target_ip[6], g_dns6_target_ip[7]
                        , g_dns6_target_ip[8], g_dns6_target_ip[9], g_dns6_target_ip[10], g_dns6_target_ip[11]
                        , g_dns6_target_ip[12], g_dns6_target_ip[13], g_dns6_target_ip[14], g_dns6_target_ip[15]
                        );

                        printf(" DNS Done\n");
                        g_dns_get_ip_flag = 1;
                    }
// If you have an available network through IPv6.
//#ifdef IPV6
#if 0
                    g_dns_get_ip_flag = 0;

                    IP_TYPE = 0x1;
                    if (DNS_run(SOCKET_DNS, g_net_info.dns6, g_dns6_target_domain, g_dns_target_ip, AS_IPV6) > 0)
                    {
                        printf(" DNS6 success\n");
                        printf(" Target domain : %s\n", g_dns6_target_domain);
                        printf(" IP of target domain : %d.%d.%d.%d\n", g_dns_target_ip[0], g_dns_target_ip[1], g_dns_target_ip[2], g_dns_target_ip[3]);

                        IP_TYPE = 0x1c;
                        if (DNS_run(SOCKET_DNS, g_net_info.dns6, g_dns6_target_domain, g_dns6_target_ip, AS_IPV6) > 0)
                        {
                            printf(" DNS6 success\n");
                            printf(" Target domain : %s\n", g_dns6_target_domain);
                            printf(" IP of target domain : %.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x:%.2x%.2x\n"
                            , g_dns6_target_ip[0], g_dns6_target_ip[1], g_dns6_target_ip[2], g_dns6_target_ip[3]
                            , g_dns6_target_ip[4], g_dns6_target_ip[5], g_dns6_target_ip[6], g_dns6_target_ip[7]
                            , g_dns6_target_ip[8], g_dns6_target_ip[9], g_dns6_target_ip[10], g_dns6_target_ip[11]
                            , g_dns6_target_ip[12], g_dns6_target_ip[13], g_dns6_target_ip[14], g_dns6_target_ip[15]
                            );

                            g_dns_get_ip_flag = 1;
                        }
                    }
#endif
                }
                else
                {
                    dns_retry++;

                    if (dns_retry <= DNS_RETRY_COUNT)
                    {
                        printf(" DNS timeout occurred and retry %d\n", dns_retry);
                    }
                }

                if (dns_retry > DNS_RETRY_COUNT)
                {
                    printf(" DNS failed\n");

                    while (1)
                        ;
                }
                
                if(g_dns_get_ip_flag == 0)
                {
                    wizchip_delay_ms(1000); // wait for 1 second
                }
            }
            #endif
        }
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

/* DHCP */
static void wizchip_dhcp4_init(void)
{
    printf(" DHCP client running\n");

    DHCPv4_init(SOCKET_DHCP, g_ethernet_buf);

    reg_dhcpv4_cbfunc(wizchip_dhcp4_assign, wizchip_dhcp4_assign, wizchip_dhcp4_conflict);
}

static void wizchip_dhcp4_assign(void)
{
    getIPfromDHCPv4(g_net_info.ip);
    getGWfromDHCPv4(g_net_info.gw);
    getSNfromDHCPv4(g_net_info.sn);
    getDNSfromDHCPv4(g_net_info.dns);

    /* Network initialize */
    network_initialize(g_net_info); // apply from DHCP

    print_network_information(g_net_info);
    printf(" DHCP leased time : %ld seconds\n", getDHCPv4Leasetime());
}

static void wizchip_dhcp4_conflict(void)
{
    printf(" Conflict IP from DHCP\n");

    // halt or reset or any...
    while (1)
        ; // this example is halt.
}

/* Timer */
static void repeating_timer_callback(void)
{
    g_msec_cnt++;

    if (g_msec_cnt >= 1000 - 1)
    {
        g_msec_cnt = 0;

        DHCPv4_time_handler();
        DNS_time_handler();
    }
}