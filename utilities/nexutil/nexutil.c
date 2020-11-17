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

#include <time.h>

#define NEX_SKAUFMANN_ECW 550
#define NEX_SKAUFMANN_PID 551
#define NEX_SKAUFMANN_READ_PID 554

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
int time_period = 500; /* in milliseconds */

static char doc[] = "timer -- program to trigger ioctl for control loop in Wifi driver";

struct argp_option options[] ={
    {"time", 't', "<integer>", 0, "Time in ms for one iteration of ioctl send to driver."},
    {0}
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    switch (key) {
        case 't':
            time_period = atoi(arg);
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, 0, doc};

struct pid_read {
    uint32_t empty_slots;
    uint32_t frame_sharing1;
    uint32_t frame_sharing2;
};
typedef struct pid_read pid_read_t;

uint32_t get_uint32_t(uint8_t *ptr) {
    uint32_t value = (ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | ptr[0];
    return value;
}

/* read measured values from driver */
void read_values(struct nexio *nexio, pid_read_t *pid_read) {
    uint8_t buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    nex_ioctl(nexio, NEX_SKAUFMANN_READ_PID, buffer, BUFFER_SIZE, true);

    pid_read->empty_slots = get_uint32_t(buffer);
    pid_read->frame_sharing1 = get_uint32_t(buffer+4);
    pid_read->frame_sharing2 = get_uint32_t(buffer+8);
    printf("empty_slots: %d, frame_sharing1: %d, frame_sharing2: %d\n", pid_read->empty_slots, pid_read->frame_sharing1, pid_read->frame_sharing2);
}

/* calculate new ecw based on measured values read from driver */
void pid_loop(pid_read_t *pid_read) {

}

/* set ecw based on calculated new parameters */
void set_ecw(pid_read_t *pid_read) {

}

int main(int argc, char **argv)
{
    if (sizeof(int) != sizeof(int32_t)) {
        printf("Warning, atoi might not return int32_t\n, sizeof(int): %d, sizeof(int32_t): %d\n", sizeof(int), sizeof(int32_t));
    }

    argp_parse(&argp, argc, argv, 0, 0, 0);

    struct nexio *nexio;
    nexio = nex_init_ioctl(ifname);

    pid_read_t pid_read;

    printf("Connect to interface '%s' and send ioctl every %dms\n", ifname, time_period);

    while (1) {
        clock_t start_time = clock();
        while (clock() < start_time + time_period * 1000);
    
        printf("Send ioctl to %s\n", ifname);

        read_values(nexio, &pid_read);
        pid_loop(&pid_read);
        set_ecw(&pid_read);
    }

    return 0;
}