/***************************************************************************
 *                                                                         *
 *          ###########   ###########   ##########    ##########           *
 *         ############  ############  ############  ############          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ###########   ####  ######  ##   ##   ##  ##    ######          *
 *          ###########  ####  #       ##   ##   ##  ##    #    #          *
 *                   ##  ##    ######  ##   ##   ##  ##    #    #          *
 *                   ##  ##    #       ##   ##   ##  ##    #    #          *
 *         ############  ##### ######  ##   ##   ##  ##### ######          *
 *         ###########    ###########  ##   ##   ##   ##########           *
 *                                                                         *
 *            S E C U R E   M O B I L E   N E T W O R K I N G              *
 *                                                                         *
 * This file is part of NexMon.                                            *
 *                                                                         *
 * Copyright (c) 2016 NexMon Team                                          *
 *                                                                         *
 * NexMon is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation, either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * NexMon is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with NexMon. If not, see <http://www.gnu.org/licenses/>.          *
 *                                                                         *
 **************************************************************************/

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <string.h>
#include <byteswap.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/param.h> // for MIN macro

#include <sys/ioctl.h>
#include <arpa/inet.h>
#ifdef BUILD_ON_RPI
#include <types.h> //not sure why it was removed, but it is needed for typedefs like `uint`
#include <linux/if.h>
#else
#include <net/if.h>
#endif
#include <stdbool.h>
#define TYPEDEF_BOOL
#include <errno.h>

#include <wlcnt.h>

#include <nexioctls.h>

#include <typedefs.h>
#include <bcmwifi_channels.h>
#include <b64.h>
#include <stdint.h>

#include <stdio.h>
#include <time.h>
#include <signal.h>

#define NEX_SKAUFMANN_ECW 550

#define BUFFER_SIZE 34

struct nexio {
    struct ifreq *ifr;
    int sock_rx_ioctl;
    int sock_rx_frame;
    int sock_tx;
};

extern int nex_ioctl(struct nexio *nexio, int cmd, void *buf, int len, bool set);
extern struct nexio *nex_init_ioctl(const char *ifname);
extern struct nexio *nex_init_udp(unsigned int securitycookie, unsigned int txip);
extern struct nexio *nex_init_netlink(void);

char *ifname = "eth6";
uint8_t ecw = 0x00;

static char doc[] = "ecw -- program to set contention window on Broadcom chips.";

struct argp_option options[] ={
    {"interface", 'i', "<string>", 0, "Interface to set ECW for"},
    {"ecw", 'c', "0x[0-9a-fA-F][0-9a-fA-F]", 0, "ECW value to set interface to"},
    {0}
};

static int is_hex_digit(char digit) {
    if ((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F')) {
        return 1;
    }
    return 0;
}

static int parse_digit(char digit) {
    if (!is_hex_digit(digit)) {
        return -1;
    }

    if (digit >= '0' && digit <= '9')   {
        return digit - '0';
    }

    if (digit >= 'a' && digit <= 'f')   {
        return digit - 'a' + 10;
    }

    if (digit >= 'A' && digit <= 'F')   {
        return digit - 'A' + 10;
    }

    return -1;
}

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    switch (key) {
        case 'i':
            ifname = arg;
            break;
        case 'c':
            if (arg[0] != '0' || arg[1] != 'x' || !is_hex_digit(arg[2]) || !is_hex_digit(arg[3])) {
                printf("ERR: -c or --ecw argument needs to start with 0x followed by two hex digits [0-9a-fA-F]\n");
                return;
            }
            ecw = (parse_digit(arg[2]) << 4) | parse_digit(arg[3]);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, 0, doc};

timer_t gTimerid;

void start_timer(void)
{
    struct itimerspec value;

    value.it_value.tv_sec = 0;
    value.it_value.tv_nsec = 500000000L;

    value.it_interval.tv_sec = 0;
    value.it_interval.tv_nsec = 500000000L;

    timer_create(CLOCK_REALTIME, NULL, &gTimerid);

    timer_settime (gTimerid, 0, &value, NULL);
}

void stop_timer(void)
{
    struct itimerspec value;

    value.it_value.tv_sec = 0;
    value.it_value.tv_nsec = 0;

    value.it_interval.tv_sec = 0;
    value.it_interval.tv_nsec = 0;

    timer_settime (gTimerid, 0, &value, NULL);
}

void timer_callback(int sig) {
    printf(" Catched timer signal: %d ... !!\n", sig);
    start_timer();
        (void) signal(SIGALRM, timer_callback);
}

int main(int argc, char **argv)
{
    while (1) {
        clock_t start_time = clock();
        while (clock() < start_time + 500 * 1000);
        
        printf("500ms\n");
    }




    argp_parse(&argp, argc, argv, 0, 0, 0);

    struct nexio *nexio;
    
    nexio = nex_init_ioctl(ifname);

    uint8_t *buffer = malloc(BUFFER_SIZE);
    memset(buffer, 0, BUFFER_SIZE);

    buffer[0] = ecw;
    
    nex_ioctl(nexio, NEX_SKAUFMANN_ECW, buffer, BUFFER_SIZE, true);

    printf("Set interface '%s' to ECW 0x%x\n", ifname, ecw);
   
    return 0;
}