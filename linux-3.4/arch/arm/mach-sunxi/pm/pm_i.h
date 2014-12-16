#ifndef _PM_I_H
#define _PM_I_H

/*
 * Copyright (c) 2011-2015 njubie@allwinnertech.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#include "pm_def_i.h"
#include "pm_types.h"
#include "pm.h"
#include "mem_mapping.h"

#include <linux/power/aw_pm.h>
#include <linux/arisc/hwmsgbox.h>
#include <linux/arisc/hwspinlock.h>

#include "standby/super/super_clock.h"
#include "standby/super/super_power.h"
#include "standby/super/super_twi.h"

#include <stdarg.h>

/* define register for interrupt controller */
typedef struct __MEM_INT_REG{

    volatile __u32   Vector;
    volatile __u32   BaseAddr;
    volatile __u32   reserved0;
    volatile __u32   NmiCtrl;

    volatile __u32   IrqPend[3];
    volatile __u32   reserved1;

    volatile __u32   FiqPend[3];
    volatile __u32   reserved2;

    volatile __u32   TypeSel[3];
    volatile __u32   reserved3;

    volatile __u32   IrqEn[3];
    volatile __u32   reserved4;

    volatile __u32   IrqMask[3];
    volatile __u32   reserved5;

    volatile __u32   IrqResp[3];
    volatile __u32   reserved6;

    volatile __u32   IrqForce[3];
    volatile __u32   reserved7;

    volatile __u32   IrqPrio[5];
} __mem_int_reg_t;


void standby_delay(int cycle);
void standby_delay_cycle(int cycle);

#endif /*_PM_I_H*/

