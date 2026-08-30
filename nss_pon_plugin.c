/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/version.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/rcupdate.h>
#include <linux/sysctl.h>
#include <linux/if_vlan.h>

#include "nss_pon_plugin_public.h"

#define NSS_PON_PLUGIN_SYSCTL_PATH "net/nss_pon_plugin"

/*
 * RCU-protected ops object, populated by nss_pon_plugin_register()/
 * nss_pon_plugin_unregister(). Neither the ECM module nor the PON/OMCI
 * driver module dereferences this directly - both go through the
 * exported functions below, which is what keeps this module independent
 * of either of them.
 */
static struct nss_pon_ops nss_pon_plugin_ops;

/*
 * When set, nss_pon_plugin_query() always returns a fixed, known response
 * instead of consulting the registered driver (or the absence of one).
 * This lets a caller's handling of the response be verified on demand,
 * independent of whatever the real PON/OMCI driver would decide.
 *
 * echo 1 > /proc/sys/net/nss_pon_plugin/nss_pon_test_en
 */
static int nss_pon_test_en;

static struct ctl_table_header *nss_pon_plugin_ctl_table_header;

/*
 * Egress VLAN ID (just the VID, not the full encoded tag) used for the
 * fixed test response below. Combined with the 802.1Q TPID at query time
 * to build the (tpid << 16) | vid tag struct nss_pon_vlan_rule expects -
 * overridable so it matches whatever VLAN the actual test setup is using.
 *
 * echo <vlan_id> > /proc/sys/net/nss_pon_plugin/nss_pon_vid
 */
static int nss_pon_vid;


/*
 * nss_pon_plugin_register()
 *	Registers PON/OMCI query callbacks.
 */
int nss_pon_plugin_register(struct nss_pon_ops *ops)
{
	if (!ops || !ops->query) {
		pr_err("%s:%d: PON/OMCI query callback is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (rcu_access_pointer(nss_pon_plugin_ops.query)) {
		pr_err("%s:%d: PON/OMCI query callback is already registered\n", __func__, __LINE__);
		return -EBUSY;
	}

	rcu_assign_pointer(nss_pon_plugin_ops.query, ops->query);
	synchronize_rcu();

	return 0;
}
EXPORT_SYMBOL(nss_pon_plugin_register);

/*
 * nss_pon_plugin_unregister()
 *	Unregisters PON/OMCI query callbacks.
 */
void nss_pon_plugin_unregister(void)
{
	rcu_assign_pointer(nss_pon_plugin_ops.query, NULL);
	synchronize_rcu();
}
EXPORT_SYMBOL(nss_pon_plugin_unregister);

/*
 * nss_pon_plugin_query_test_response()
 *	Fills in a fixed, known response, used by the nss_pon_test_en
 *	sysctl to sanity check a caller's handling of nss_pon_plugin_query()
 *	independent of whether/what a real driver is registered.
 */
static void nss_pon_plugin_query_test_response(struct nss_pon_plugin_query_info *info, struct nss_pon_action *action)
{
	action->accel_allowed = true;
	action->in_vlan_action = NSS_PON_INGRESS_VLAN_MATCH_AGNOSTIC;

	if (info->direction == NSS_PON_FLOW_DIRECTION_PON_TO_UNI) {
		action->eg_vlan_action = NSS_PON_EGRESS_VLAN_ACTION_UNTAG;
		return;
	}

	action->eg_vlan_action = NSS_PON_EGRESS_VLAN_ACTION_SINGLE;
	action->eg_vlan_tag.primary_vlan_tag = (ETH_P_8021Q << 16) | (nss_pon_vid & VLAN_VID_MASK);
}

/*
 * nss_pon_plugin_query()
 *	Invokes the registered PON/OMCI query callback, if any.
 */
bool nss_pon_plugin_query(struct nss_pon_plugin_query_info *info, struct nss_pon_action *action)
{
	nss_pon_query_t query;
	bool ret;

	if (nss_pon_test_en) {
		nss_pon_plugin_query_test_response(info, action);
		return true;
	}

	rcu_read_lock();
	query = rcu_dereference(nss_pon_plugin_ops.query);
	if (!query) {
		rcu_read_unlock();
		return false;
	}

	ret = query(info, action);
	rcu_read_unlock();

	return ret;
}
EXPORT_SYMBOL(nss_pon_plugin_query);

/*
 * nss_pon_plugin_is_registered()
 *	Reports whether a PON/OMCI driver is currently registered.
 */
bool nss_pon_plugin_is_registered(void)
{
	return !!rcu_access_pointer(nss_pon_plugin_ops.query);
}
EXPORT_SYMBOL(nss_pon_plugin_is_registered);

/*
 * nss_pon_plugin_init()
 */
static int __init nss_pon_plugin_init(void)
{
	static struct ctl_table nss_pon_plugin_ctl_table[] = {
		{
			.procname	= "nss_pon_test_en",
			.data		= &nss_pon_test_en,
			.maxlen		= sizeof(int),
			.mode		= 0644,
			.proc_handler	= &proc_dointvec,
		},
		{
			.procname	= "nss_pon_vid",
			.data		= &nss_pon_vid,
			.maxlen		= sizeof(int),
			.mode		= 0644,
			.proc_handler	= &proc_dointvec,
		},
		{ }
	};

	nss_pon_plugin_ctl_table_header = register_sysctl(NSS_PON_PLUGIN_SYSCTL_PATH, nss_pon_plugin_ctl_table);
	if (!nss_pon_plugin_ctl_table_header) {
		pr_err("PON plugin: failed to register sysctl\n");
		return -ENOMEM;
	}

	pr_info("PON plugin loaded\n");
	return 0;
}

/*
 * nss_pon_plugin_exit()
 */
static void __exit nss_pon_plugin_exit(void)
{
	if (nss_pon_plugin_ctl_table_header) {
		unregister_sysctl_table(nss_pon_plugin_ctl_table_header);
	}

	pr_info("PON plugin unloaded\n");
}

module_init(nss_pon_plugin_init)
module_exit(nss_pon_plugin_exit)

MODULE_DESCRIPTION("PON Plugin");
MODULE_LICENSE("Dual BSD/GPL");
