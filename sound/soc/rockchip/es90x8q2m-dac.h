/*
 * Driver for ES90x8Q2M
 *
 * Author: Xiao
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef _SND_SOC_ES90X8Q2M_I2C
#define _SND_SOC_ES90X8Q2M_I2C


/* ES90X8Q2M_I2C Register Address */
#define ES9028Q2M_REG_01	0x01	/* Virtual Device ID  :  0x01 = ES90x8Q2M */
#define ES9028Q2M_REG_02	0x02	/* API revision       :  0x01 = Revision 01 */
#define ES9028Q2M_REG_10	0x10	/* 0x01 = 352.8kHz or 384kHz, 0x00 = otherwise */
#define ES9028Q2M_REG_20	0x20	/* 0 - 100 (decimal value, 0 = min., 100 = max.) */
#define ES9028Q2M_REG_21	0x21	/* 0x00 = Mute OFF, 0x01 = Mute ON */
#define ES9028Q2M_REG_22	0x22	/* 0x00 = Fast Roll-Off, 0x01 = Slow Roll-Off, 0x02 = Minimum Phase */
#define ES9028Q2M_REG_23	0x23	/* 0x00 = 47.44k, 0x01 = 50kHz, 0x02 = 60kHz, 0x03 = 70kHz */
#define ES9028Q2M_MAX_REG	0x23	/* Maximum Register Number */

#endif /* _SND_SOC_ES90X8Q2M_I2C */
