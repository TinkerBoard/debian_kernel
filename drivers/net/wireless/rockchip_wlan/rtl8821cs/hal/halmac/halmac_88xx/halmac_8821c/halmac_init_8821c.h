/* SPDX-License-Identifier: GPL-2.0 */
/******************************************************************************
 *
 * Copyright(c) 2016 - 2019 Realtek Corporation. All rights reserved.
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
 ******************************************************************************/

#ifndef _HALMAC_INIT_8821C_H_
#define _HALMAC_INIT_8821C_H_

#include "../../halmac_api.h"

#if HALMAC_8821C_SUPPORT

enum halmac_ret_status
mount_api_8821c(struct halmac_adapter *adapter);

enum halmac_ret_status
init_trx_cfg_8821c(struct halmac_adapter *adapter, enum halmac_trx_mode mode);

enum halmac_ret_status
init_system_cfg_8821c(struct halmac_adapter *adapter);

enum halmac_ret_status
init_protocol_cfg_8821c(struct halmac_adapter *adapter);

enum halmac_ret_status
init_h2c_8821c(struct halmac_adapter *adapter);

enum halmac_ret_status
init_edca_cfg_8821c(struct halmac_adapter *adapter);

enum halmac_ret_status
init_wmac_cfg_8821c(struct halmac_adapter *adapter);

enum halmac_ret_status
pre_init_system_cfg_8821c(struct halmac_adapter *adapter);

#endif /* HALMAC_8821C_SUPPORT */

#endif/* _HALMAC_INIT_8821C_H_ */
