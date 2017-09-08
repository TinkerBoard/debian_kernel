/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/
#ifndef __PCI_OPS_H_
#define __PCI_OPS_H_


#if defined(CONFIG_RTL8812A) || defined(CONFIG_RTL8821A)
u32	rtl8812ae_init_desc_ring(_adapter *padapter);
u32	rtl8812ae_free_desc_ring(_adapter *padapter);
void	rtl8812ae_reset_desc_ring(_adapter *padapter);
int	rtl8812ae_interrupt(PADAPTER Adapter);
void	rtl8812ae_xmit_tasklet(void *priv);
void	rtl8812ae_recv_tasklet(void *priv);
void	rtl8812ae_prepare_bcn_tasklet(void *priv);
void	rtl8812ae_set_intf_ops(struct _io_ops	*pops);
#endif

#ifdef CONFIG_RTL8814A
u32	rtl8814ae_init_desc_ring(_adapter *padapter);
u32	rtl8814ae_free_desc_ring(_adapter *padapter);
void	rtl8814ae_reset_desc_ring(_adapter *padapter);
int	rtl8814ae_interrupt(PADAPTER Adapter);
void	rtl8814ae_xmit_tasklet(void *priv);
void	rtl8814ae_recv_tasklet(void *priv);
void	rtl8814ae_prepare_bcn_tasklet(void *priv);
void	rtl8814ae_set_intf_ops(struct _io_ops	*pops);
#endif

#endif

