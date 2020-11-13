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

#define NEX_SKAUFMANN_ECW 550
#define NEX_SKAUFMANN_PID 551
#define NEX_SKAUFMANN_SET_PARAMETER 552
#define NEX_SKAUFMANN_PRINT_PARAMETERS 553

#define BUFFER_SIZE 34

#define FALSE 0
#define TRUE 1

#define RESET_INTEGRAL_INDEX 1
#define KP_INDEX 2
#define KP_DENOMINATOR_INDEX 3
#define KI_INDEX 4
#define KI_DENOMINATOR_INDEX 5
#define INTEGRAL_FACTOR_INDEX 6
#define INTEGRAL_FACTOR_DENOMINATOR_INDEX 7

char **parameter_name = {"None", "Reset Integral", "Kp", "Kp_denominator", "Ki", "Ki_denominator", "Integral Factor", "Integral Factor_denominator"};

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

int set_ecw = FALSE;
char *ifname = "wl1.1";
uint8_t ecw = 0xaa;
uint16_t txop = 0x00;

int set_kp = FALSE;
int32_t kp = 0;
int set_kp_denominator = FALSE;
int32_t kp_denominator = 0;
int set_ki = FALSE;
int32_t ki = 0;
int set_ki_denominator = FALSE;
int32_t ki_denominator = 0;
int set_integral_factor = FALSE;
int32_t integral_factor = 0;
int set_integral_factor_denominator = FALSE;
int32_t integral_factor_denominator = 0;
int reset_integral = FALSE;
int display_parameters = FALSE;

static char doc[] = "ecw -- program to set contention window on Broadcom chips.";

struct argp_option options[] ={
    {"interface", 'i', "<string>", 0, "Interface to set ECW for"},
    {"ecw", 'c', "0x[0-9a-fA-F][0-9a-fA-F]", 0, "8-bit ECW value to set interface to"},
    {"txop", 't', "0xaaaa", 0, "16-bit TXOP value to set interface to"},
    {"ki", 'k', "<integer>", 0, "I-part of the PI controller"},
    {"ki_denominator", 'K', "<integer>", 0, "Denominator of I-part of PI controller"},
    {"kp", 'p', "<integer>", 0, "P-part of the PI controller"},
    {"kp_denominator", 'P', "<integer>", 0, "Denominator of P-part of PI controller"},
    {"integral_factor", 'f', "<integer>", 0, "Integral factor f for adding the integrate part (I_n+1 = I_n * f + error)"},
    {"integral_factor_denominator", 'F', "<integer>", 0, "Integral factor denominator"},
    {"reset_integral", 'r', 0, 0, "Reset the integral value in the firmware"},
    {"display_parameters", 'd', 0, 0, "Print control parameters from Firmware"},
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
            set_ecw = TRUE;
            if (arg[0] != '0' || arg[1] != 'x' || !is_hex_digit(arg[2]) || !is_hex_digit(arg[3])) {
                printf("ERR: -c or --ecw argument needs to start with 0x followed by two hex digits [0-9a-fA-F]\n");
                return;
            }
            ecw = (parse_digit(arg[2]) << 4) | parse_digit(arg[3]);
            break;
        case 't':
            set_ecw = TRUE;
                        if (arg[0] != '0' || arg[1] != 'x' || !is_hex_digit(arg[2]) || !is_hex_digit(arg[3]) || !is_hex_digit(arg[4]) || !is_hex_digit(arg[5])) {
                printf("ERR: -t or --txop argument needs to start with 0x followed by four hex digits [0-9a-fA-F]\n");
                return;
            }
            txop = (parse_digit(arg[2]) << 12) | (parse_digit(arg[3]) << 8) | (parse_digit(arg[4]) << 4) | parse_digit(arg[5]);
        case 'k':
            ki = atoi(arg);
            set_ki = TRUE;
            break;
        case 'K':
            ki_denominator = atoi(arg);
            set_ki_denominator = TRUE;
            break;
        case 'p':
            kp = atoi(arg);
            set_kp = TRUE;
            break;
        case 'P':
            kp_denominator = atoi(arg);
            set_kp_denominator = TRUE;
            break;
        case 'f':
            integral_factor = atoi(arg);
            set_integral_factor = TRUE;
            break;
        case 'F':
            integral_factor_denominator = atoi(arg);
            set_integral_factor_denominator = TRUE;
            break;
        case 'r':
            reset_integral = TRUE;
            break;
        case 'd':
            display_parameters = TRUE;
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, 0, doc};

void set_parameter(struct nexio *nexio, int index, int32_t value) {
    uint8_t buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    buffer[0] = index;
    buffer[1] = (value >> 24) & 0xff;
    buffer[2] = (value >> 16) & 0xff;
    buffer[3] = (value >> 8) & 0xff;
    buffer[4] = value & 0xff;

    nex_ioctl(nexio, NEX_SKAUFMANN_SET_PARAMETER, buffer, BUFFER_SIZE, true);

    printf("Parameter '%s' set to value '%d'\n", parameter_name[index], value);
}

void set_parameters(struct nexio *nexio) {
    if (set_kp) {
        set_parameter(nexio, KP_INDEX, kp);
    }
    if (set_kp_denominator) {
        set_parameter(nexio, KP_DENOMINATOR_INDEX, kp_denominator);
    }
    if (set_ki) {
        set_parameter(nexio, KI_INDEX, ki);
    }
    if (set_ki_denominator) {
        set_parameter(nexio, KI_DENOMINATOR_INDEX, ki_denominator);
    }
    if (set_integral_factor) {
        set_parameter(nexio, INTEGRAL_FACTOR_INDEX, integral_factor);
    }
    if (set_integral_factor_denominator) {
        set_parameter(nexio, INTEGRAL_FACTOR_DENOMINATOR_INDEX, integral_factor_denominator);
    }
    if (reset_integral) {
        set_parameter(nexio, RESET_INTEGRAL_INDEX, 0);
    }
}

void set_ecw_parameter(struct nexio *nexio) {
    if (set_ecw) {
        uint8_t buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        buffer[0] = ecw;
        buffer[1] = txop & 0xff;
        buffer[2] = (txop >> 8) & 0xff;
        
        nex_ioctl(nexio, NEX_SKAUFMANN_ECW, buffer, BUFFER_SIZE, true);

        printf("Set interface '%s' to ECW 0x%x and TXOP to 0x%x\n", ifname, ecw, txop);
    }
}

void send_display_parameters(struct nexio *nexio) {
    if (display_parameters) {
        uint8_t buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);

        nex_ioctl(nexio, NEX_SKAUFMANN_PRINT_PARAMETERS, buffer, BUFFER_SIZE, true);

        printf("Sent ioctl request to firmware, execute 'dmesg' to see printed parameters\n");
    }
}

int main(int argc, char **argv)
{
    if (sizeof(int) != sizeof(int32_t)) {
        printf("Warning, atoi might not return int32_t\n, sizeof(int): %d, sizeof(int32_t): %d\n", sizeof(int), sizeof(int32_t));
    }

    argp_parse(&argp, argc, argv, 0, 0, 0);

    struct nexio *nexio;
    nexio = nex_init_ioctl(ifname);

    set_parameters(nexio);

    set_ecw_parameter(nexio);

    send_display_parameters(nexio);
   
    return 0;
}
