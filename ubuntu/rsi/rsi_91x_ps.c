/**
 * Copyright (c) 2014 Redpine Signals Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/etherdevice.h>
#include <linux/if.h>
#include <linux/version.h>
#include "rsi_debugfs.h"
#include "rsi_mgmt.h"
#include "rsi_common.h"
#include "rsi_ps.h"

/**
 * str_psstate() - return the ps state in string format.
 *
 * @state - PS state.
 *
 * return: PS state in string format.
 */
char *str_psstate(enum ps_state state)
{
	switch (state) {
	case PS_NONE:
		return "PS_NONE";
	case PS_DISABLE_REQ_SENT:
		return "PS_DISABLE_REQ_SENT";
	case PS_ENABLE_REQ_SENT:
		return "PS_ENABLE_REQ_SENT";
	case PS_ENABLED:
		return "PS_ENABLED";
	default:
		return "INVALID_STATE";
	}
	return "INVALID_STATE";
}

/**
 * rsi_modify_ps_state() - Modify PS state to a new state.
 *
 * @adapter: pointer to rsi_hw structure.
 * @nstate: new PS state.
 *
 * return: new state.
 */
static inline void rsi_modify_ps_state(struct rsi_hw *adapter,
				       enum ps_state nstate)
{
	ven_rsi_dbg(INFO_ZONE, "PS state changed %s => %s\n",
		str_psstate(adapter->ps_state),
		str_psstate(nstate));

	adapter->ps_state = nstate;
}

/**
 * rsi_default_ps_params() - Initalization of default powersave parameters.
 *
 * @adapter: pointer to rsi_hw structure.
 *
 * return: void.
 */
void rsi_default_ps_params(struct rsi_hw *adapter)
{
	struct rsi_ps_info *ps_info = &adapter->ps_info;

	ps_info->enabled = true;
	ps_info->sleep_type = 1; /* LP */
	ps_info->tx_threshold = 0;
	ps_info->rx_threshold = 0;
	ps_info->tx_hysterisis = 0;
	ps_info->rx_hysterisis = 0;
	ps_info->monitor_interval = 0;
	ps_info->listen_interval = 2 * 100;
	ps_info->num_bcns_per_lis_int = 0;
	ps_info->dtim_interval_duration = 0;
	ps_info->num_dtims_per_sleep = 0;
	ps_info->deep_sleep_wakeup_period = 100;
}
EXPORT_SYMBOL_GPL(rsi_default_ps_params);

/**
 * rsi_enable_ps() - enable power save
 *
 * @adapter: Pointer to rsi_hw structure.
 *
 * return: void.
 */
void rsi_enable_ps(struct rsi_hw *adapter)
{
	if (adapter->ps_state != PS_NONE) {
		ven_rsi_dbg(ERR_ZONE,
			"%s: Cannot accept enable PS in %s state\n",
			__func__, str_psstate(adapter->ps_state));
		return;
	}

	if (rsi_send_ps_request(adapter, true)) {
		ven_rsi_dbg(ERR_ZONE,
			"%s: Failed to send PS request to device\n",
			__func__);
		return;
	}

	rsi_modify_ps_state(adapter, PS_ENABLE_REQ_SENT);
}

/**
 * rsi_disable_ps() - disable power save
 *
 * @adapter: Pointer to rsi_hw structure.
 *
 * return: void.
 */
void rsi_disable_ps(struct rsi_hw *adapter)
{
	if (adapter->ps_state != PS_ENABLED) {
		ven_rsi_dbg(ERR_ZONE,
			"%s: Cannot accept disable PS in %s state\n",
			__func__, str_psstate(adapter->ps_state));
		return;
	}

	if (rsi_send_ps_request(adapter, false)) {
		ven_rsi_dbg(ERR_ZONE,
			"%s: Failed to send PS request to device\n",
			__func__);
		return;
	}

	rsi_modify_ps_state(adapter, PS_DISABLE_REQ_SENT);
}

/**
 * rsi_conf_uapsd() - configures UAPSD powersave.
 *
 * @adapter - Pointer to rsi_hw structure.
 *
 * return: void.
 */
void rsi_conf_uapsd(struct rsi_hw *adapter)
{
	if (adapter->ps_state != PS_ENABLED)
		return;

	if (rsi_send_ps_request(adapter, false)) {
		ven_rsi_dbg(ERR_ZONE,
			"%s: Failed to send PS request to device\n",
			__func__);
		return;
	}

	if (rsi_send_ps_request(adapter, true)) {
		ven_rsi_dbg(ERR_ZONE,
			"%s: Failed to send PS request to device\n",
			__func__);
	}
}

/**
 * rsi_handle_ps_confirm() - Processes powersave confirmation.
 *
 * @adapter - Pointer to rsi_hw structure.
 * @msg - Recevied buffer.
 *
 * return: 0 on success.
 */
int rsi_handle_ps_confirm(struct rsi_hw *adapter, u8 *msg)
{
	u16 cfm_type = 0;

	cfm_type = *(u16 *)&msg[PS_CONFIRM_INDEX];

	switch (cfm_type) {
	case SLEEP_REQUEST:
		if (adapter->ps_state == PS_ENABLE_REQ_SENT)
			rsi_modify_ps_state(adapter, PS_ENABLED);
		break;
	case WAKEUP_REQUEST:
		if (adapter->ps_state == PS_DISABLE_REQ_SENT)
			rsi_modify_ps_state(adapter, PS_NONE);
		break;
	default:
		ven_rsi_dbg(ERR_ZONE,
			"Invalid PS confirm type %x in state %s\n",
			cfm_type, str_psstate(adapter->ps_state));
		return -1;
	}

	return 0;
}
