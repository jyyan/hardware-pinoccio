/* Copyright (c) 2007 Axel Wachtler
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
   * Neither the name of the authors nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE. */

/* $Id$ */
/**
 * @file
 * @brief Implementation of the control commands for @ref grpAppSniffer
 *
 * @ingroup grpAppSniffer
 */
/* === includes ============================================================ */
#include "sniffer.h"

/* === macros ============================================================== */

/* === types =============================================================== */

/** supported command codes for sniffer application
 *
 * This enumeration is generated by:
 * Tools/cmdhash.py parms cmset cmclr cmask chan cpage ed scan sniff idle
 */
typedef enum {
     /** Hashvalue for command 'chkcrc' */
     CMD_CHKCRC = 0x23,
     /** Hashvalue for command 'timeset' */
     CMD_TIMESET = 0x35,
     /** Hashvalue for command 'scan' */
     CMD_SCAN = 0x4d,
     /** Hashvalue for command 'chan' */
     CMD_CHAN = 0x4e,
     /** Hashvalue for command 'cmclr' */
     CMD_CMCLR = 0x52,
     /** Hashvalue for command 'sniff' */
     CMD_SNIFF = 0x64,
     /** Hashvalue for command 'cmset' */
     CMD_CMSET = 0x74,
     /** Hashvalue for command 'cpage' */
     CMD_CPAGE = 0x87,
     /** Hashvalue for command 'drate' */
     CMD_DRATE = 0xa0,
     /** Hashvalue for command 'idle' */
     CMD_IDLE = 0xa1,
     /** Hashvalue for command 'cmask' */
     CMD_CMASK = 0xa9,
     /** Hashvalue for command 'ed' */
     CMD_ED = 0xc7,
     /** Hashvalue for command 'parms' */
     CMD_PARMS = 0xf1,
     /** Hashvalue for command 'sfd' */
     CMD_SFD = 0xc4,
     /** Hashvalue for empty command  */
     CMD_EMPTY = 0x00,
} SHORTENUM cmd_hash_t;

/* === globals ============================================================= */

/* === prototypes ========================================================== */
static bool process_hotkey(char cmdkey);
static bool process_command(char * cmd);
static cmd_hash_t get_cmd_hash(char *cmd);

/* === functions =========================================================== */

void ctrl_process_input(void)
{
bool success;
int inchar;
static char  cmdline[16];
static uint8_t  cmdidx = 0;

    /* command processing */
    inchar = hif_getc();
    if(EOF != inchar)
    {
        cmdline[cmdidx++] = (char) inchar;
        if (inchar == '\n' || inchar == '\r')
        {
            cmdline[cmdidx-1] = 0;
            if (cmdidx > 2)
            {
                process_command(cmdline);
            }
            cmdidx = 0;
        }
        else if (cmdidx == 1)
        {
           hif_putc('\r');
           success = process_hotkey(cmdline[0]);
           if (success == true)
           {
                cmdidx = 0;
           }
        }
    }
    if (cmdidx >= sizeof(cmdline)-1)
    {
        cmdidx = 0;
        cmdline[0] = 0;
    }
}

uint8_t cnt_active_channels(uint32_t cmask)
{
uint8_t ret = 0;

    while (cmask != 0)
    {
        ret += (uint8_t)(cmask & 1UL);
        cmask >>= 1;
    }
    return ret;
}

/**
 * @brief Key processing.
 */
static bool process_hotkey(char cmdkey)
{
    bool ret = true;
    sniffer_state_t next_state;
    next_state = ctx.state;
    switch (cmdkey)
    {
        case 'I':
        case ' ': /* panic mode switch off: hit space bar*/
            next_state = IDLE;
            PRINT("IDLE\n");
            break;

        case '+':
            if (ctx.state != SCAN)
            {
                ctx.cchan = (ctx.cchan >= TRX_MAX_CHANNEL) ? TRX_MIN_CHANNEL : ctx.cchan + 1;
                trx_bit_write(SR_CHANNEL, ctx.cchan);
                PRINTF("Channel %u"NL, ctx.cchan);
            }
            break;

        case '-':
            if (ctx.state != SCAN)
            {
                ctx.cchan = (ctx.cchan <= TRX_MIN_CHANNEL) ? TRX_MAX_CHANNEL : ctx.cchan - 1;
                trx_bit_write(SR_CHANNEL, ctx.cchan);
                PRINTF("Channel %u"NL, ctx.cchan);
            }
            break;

        case 'r':
            if (ctx.scanres_reset != 0)
            {
                ctx.scanres_reset = 0;
            }
            else
            {
                ctx.scanres_reset = cnt_active_channels(ctx.cmask);
            }
            break;

        case 'R':
            if (ctx.scanres_reset != 0)
            {
                ctx.scanres_reset = 0;
            }
            else
            {
                ctx.scanres_reset = TRX_NB_CHANNELS + 1;
            }
            break;

        default:
            ret = false;
            break;
    }

    if (next_state != ctx.state)
    {
        sniffer_stop();
        sniffer_start(next_state);
    }

    return ret;
}

/**
 * @brief Command processing.
 */
static bool process_command(char * cmd)
{
    char *argv[4];
    uint8_t argc;
    uint8_t ch, tmp;
    volatile uint8_t i;
    bool cmdok;
    sniffer_state_t next_state;
    time_t tv;
    next_state = ctx.state;

    PRINTF("> %s"NL,cmd);
    argc = hif_split_args(cmd, sizeof(argv), argv);
    ch = get_cmd_hash(argv[0]);
    cmdok = true;
    switch (ch)
    {
        case CMD_TIMESET:
            PRINTF("\n\rct1=%ld\n\r", timer_systime());
            tv = strtol(argv[1],NULL,10);
            PRINTF("tv=%ld %s\n\r", tv , argv[1]);
            timer_set_systime(tv);
            PRINTF("ct2=%ld\n\r", timer_systime());
            break;
        case CMD_CHAN:
            tmp = atoi(argv[1]);
            if (tmp>= TRX_MIN_CHANNEL && tmp <= TRX_MAX_CHANNEL)
            {
                ctx.cchan = tmp;
                trx_bit_write(SR_CHANNEL, ctx.cchan);
            }
            else
            {
                cmdok = false;
            }
            break;
        case CMD_CPAGE:
            ctx.cpage = atoi(argv[1]);
            break;
        case CMD_CMASK:
            ctx.cmask = (strtol(argv[1],NULL,0) & TRX_SUPPORTED_CHANNELS);
            break;
        case CMD_CMCLR:
            if (argc < 2)
            {
                ctx.cmask &= ~TRX_SUPPORTED_CHANNELS;
            }
            else
            {
                ctx.cmask &= ~(uint32_t)(1UL << (atoi(argv[1])));
            }
            break;
        case CMD_CMSET:
            if (argc < 2)
            {
                ctx.cmask |= TRX_SUPPORTED_CHANNELS;
            }
            else
            {
                ctx.cmask |= (uint32_t)((1UL << (atoi(argv[1]))) & TRX_SUPPORTED_CHANNELS);
            }
            break;
        case CMD_PARMS:
            PRINTF(NL"PLATFORM: %s V%s"NL, BOARD_NAME, VERSION);
            PRINTF("SUPP_CMSK: 0x%08lx"NL, (unsigned long)TRX_SUPPORTED_CHANNELS);
            PRINTF("CURR_CMSK: 0x%08lx"NL, (unsigned long)ctx.cmask);
            PRINTF("CURR_CHAN: %d"NL, ctx.cchan);
            PRINTF("CURR_PAGE: %d"NL"CURR_RATE: ", ctx.cpage);
            hif_puts_p(trx_decode_datarate_p(trx_get_datarate()));
            PRINT(NL"SUPP_RATES: ");
            for (i=0;i<trx_get_number_datarates();i++)
            {
                hif_puts_p(trx_get_datarate_str_p(i));
                PRINT(" ");
            }
            PRINT(NL);
            PRINTF("TIMER_SCALE: %d.0/%ld"NL, HWTMR_PRESCALE, F_CPU);
            PRINTF("TICK_NUMBER: %ld"NL, HWTIMER_TICK_NB);
            PRINTF("CHKCRC: %d"NL, ctx.chkcrc);
            PRINTF("MISSED_FRAMES: %d"NL,ctx.missed_frames);
            break;
        case CMD_SCAN:
            next_state = SCAN;
            break;
        case CMD_SNIFF:
            next_state = SNIFF;
            break;
        case CMD_IDLE:
            next_state = IDLE;
            break;
        case CMD_DRATE:
            tmp = get_cmd_hash(argv[1]);
            if (trx_set_datarate(tmp) == RATE_NONE)
            {
                PRINTF("Invalid datarate: %s [0x%02x]"NL,argv[1], tmp);
                cmdok = false;
            }
            break;
        case CMD_CHKCRC:
            if (argc < 2)
            {
                ctx.chkcrc ^= 1;
            }
            else if (atoi(argv[1]) != 0)
            {
                ctx.chkcrc = 1;
            }
            else
            {
                ctx.chkcrc = 0;
            }
            PRINTF("crc_ok=%d"NL,ctx.chkcrc );
            break;
        case CMD_EMPTY:
            break;
        default:
            cmdok = false;
            break;
    }

    if(cmdok != true)
    {
        PRINTF("invalid cmd, argc=%d, argv[0]=%s, hash=0x%02x"NL,
                argc, argv[0], ch);
    }
    else
    {
        PRINT("OK");
    }

    if (next_state != ctx.state)
    {
        sniffer_stop();
        sniffer_start(next_state);
    }

    return false;
}

static cmd_hash_t get_cmd_hash(char *cmd)
{
cmd_hash_t h, accu;

    h = 0;
    while (*cmd != 0)
    {
        accu = h & 0xe0;
        h = h << 5;
        h = h ^ (accu >> 5);
        h = h ^ *cmd;
        h &= 0xff;
        cmd ++;
    }
    return h;
}
/* EOF */
