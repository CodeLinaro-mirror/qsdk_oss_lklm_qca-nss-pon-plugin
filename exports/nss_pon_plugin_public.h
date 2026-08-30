/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/**
 * @file nss_pon_plugin_public.h
 *	ECM<->PON/OMCI acceleration query plugin.
 *
 * This header is the entire contract between the ECM module and the
 * PON/OMCI driver module. Neither module includes anything from the
 * other; both include only this header and link against this plugin
 * module's exported symbols.
 */

#ifndef __NSS_PON_PLUGIN_PUBLIC_H__
#define __NSS_PON_PLUGIN_PUBLIC_H__

#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/types.h>

/**
 * @addtogroup nss_pon_plugin_subsystem
 * @{
 */

/**
 * Egress VLAN action to be applied by an acceleration engine.
 */
enum nss_pon_egress_vlan_action {
	NSS_PON_EGRESS_VLAN_ACTION_AGNOSTIC = 1,	/**< Leave egress VLAN handling to the acceleration's engine. */
	NSS_PON_EGRESS_VLAN_ACTION_UNTAG,		/**< Strip any VLAN tag(s) on egress. */
	NSS_PON_EGRESS_VLAN_ACTION_SINGLE,		/**< Apply/translate single VLAN tag. */
	NSS_PON_EGRESS_VLAN_ACTION_DOUBLE,		/**< Apply/translate outer+inner (QinQ) VLAN tags. */
};

typedef enum nss_pon_egress_vlan_action nss_pon_egress_vlan_action_t;

/**
 * Ingress VLAN action to be applied by an acceleration engine.
 */
enum nss_pon_ingress_vlan_action {
	NSS_PON_INGRESS_VLAN_MATCH_AGNOSTIC = 1,	/**< Match has to be agnostic to VLAN. */
	NSS_PON_INGRESS_VLAN_MATCH_SINGLE,		/**< Match/translate single VLAN tag. */
	NSS_PON_INGRESS_VLAN_MATCH_DOUBLE,		/**< Match/translate outer+inner (QinQ) VLAN tags. */
};

typedef enum nss_pon_ingress_vlan_action nss_pon_ingress_vlan_action_t;

/**
 * Topological direction of a flow relative to the PON/UNI boundary.
 */
enum nss_pon_flow_direction {
	NSS_PON_FLOW_DIRECTION_UNI_TO_UNI = 1,	/**< Neither side of the flow is a PON/VEIP interface. */
	NSS_PON_FLOW_DIRECTION_UNI_TO_PON,	/**< Flow's 'to' side (flow destination) is a VEIP/GEM interface. */
	NSS_PON_FLOW_DIRECTION_PON_TO_UNI,	/**< Flow's 'from' side (flow source) is a VEIP/GEM interface. */
};

typedef enum nss_pon_flow_direction nss_pon_flow_direction_t;

/**
 * IPv4 flow/return tuple, independent of any acceleration engine's wire format.
 */
struct nss_pon_ipv4_tuple {
	__be32 flow_ip;		/**< Flow IP address. */
	__be32 return_ip;	/**< Return IP address. */
	__be16 flow_ident;	/**< Flow identifier, e.g., TCP/UDP port. */
	__be16 return_ident;	/**< Return identifier, e.g., TCP/UDP port. */
	u8 protocol;		/**< Protocol number. */
};

/**
 * IPv4 connection rule, independent of any acceleration engine's wire format.
 */
struct nss_pon_ipv4_connection_rule {
	u8 flow_mac[ETH_ALEN];			/**< Flow MAC address. */
	u8 return_mac[ETH_ALEN];		/**< Return MAC address. */
	s32 flow_interface_num;			/**< Flow interface number. */
	s32 return_interface_num;		/**< Return interface number. */
	s32 flow_top_interface_num;		/**< Top flow interface number. */
	s32 return_top_interface_num;		/**< Top return interface number. */
	__be32 flow_ip_xlate;			/**< Translated flow IP address. */
	__be32 return_ip_xlate;			/**< Translated return IP address. */
	__be16 flow_ident_xlate;		/**< Translated flow identifier, e.g., port. */
	__be16 return_ident_xlate;		/**< Translated return identifier, e.g., port. */
};

/**
 * IPv6 flow/return tuple, independent of any acceleration engine's wire format.
 */
struct nss_pon_ipv6_tuple {
	__be32 flow_ip[4];	/**< Flow IP address. */
	__be32 return_ip[4];	/**< Return IP address. */
	__be16 flow_ident;	/**< Flow identifier, e.g., TCP/UDP port. */
	__be16 return_ident;	/**< Return identifier, e.g., TCP/UDP port. */
	u8 protocol;		/**< Protocol number. */
};

/**
 * IPv6 connection rule, independent of any acceleration engine's wire format.
 */
struct nss_pon_ipv6_connection_rule {
	u8 flow_mac[ETH_ALEN];			/**< Flow MAC address. */
	u8 return_mac[ETH_ALEN];		/**< Return MAC address. */
	s32 flow_interface_num;			/**< Flow interface number. */
	s32 return_interface_num;		/**< Return interface number. */
	s32 flow_top_interface_num;		/**< Top flow interface number. */
	s32 return_top_interface_num;		/**< Top return interface number. */
	__be32 flow_ip_xlate[4];		/**< Translated flow IP address. */
	__be32 return_ip_xlate[4];		/**< Translated return IP address. */
	__be16 flow_ident_xlate;		/**< Translated flow identifier, e.g., port. */
	__be16 return_ident_xlate;		/**< Translated return identifier, e.g., port. */
};

/**
 * VLAN tag(s), independent of any acceleration engine's wire format.
 *
 * A single-tagged packet/action uses only primary_vlan_tag; a double
 * (QinQ) tagged packet/action uses both.
 */
struct nss_pon_vlan_rule {
	u32 primary_vlan_tag;	/**< Outer/primary VLAN tag. */
	u32 secondary_vlan_tag;	/**< Inner/secondary (QinQ) VLAN tag. */
};

/**
 * Query info passed INTO the PON/OMCI driver for a new flow.
 *
 * All fields are plugin-owned types so that this query is not tied to any
 * one acceleration engine's wire format; the acceleration engine populates
 * these from its own rule structures before invoking the query.
 */
struct nss_pon_plugin_query_info {
	struct sk_buff *skb;			/**< skb of the packet that triggered acceleration. */

	bool is_v6;				/**< true = IPv6 flow, false = IPv4 flow. */
	nss_pon_flow_direction_t direction;	/**< Topological direction of the flow relative to the PON/UNI boundary. */
	bool is_routed;				/**< true = routed connection, false = bridged connection. */
	union {
		struct {
			struct nss_pon_ipv4_tuple tuple;
			struct nss_pon_ipv4_connection_rule conn_rule;
		} v4;
		struct {
			struct nss_pon_ipv6_tuple tuple;
			struct nss_pon_ipv6_connection_rule conn_rule;
		} v6;
	};

	struct nss_pon_vlan_rule vlan_tag;	/**< VLAN tag(s) already present on the packet. */
};

/**
 * Action decision returned FROM the PON/OMCI driver.
 */
struct nss_pon_action {
	bool accel_allowed;				/**< Accelerate / do-not-accelerate. */
	nss_pon_egress_vlan_action_t eg_vlan_action;	/**< Egress VLAN action to apply. */
	nss_pon_ingress_vlan_action_t in_vlan_action;	/**< Ingress VLAN action to apply. */

	struct nss_pon_vlan_rule eg_vlan_tag;		/**< Egress VLAN tag(s) to apply. */
	struct nss_pon_vlan_rule in_vlan_tag;		/**< Ingress VLAN tag(s) to apply. */
};

/**
 * Callback that queries the PON/OMCI driver for the acceleration decision of a new flow.
 *
 * @param	info	The flow information gathered by the acceleration engine.
 * @param	action	The decision filled in by the PON/OMCI driver.
 *
 * @return
 * true if the PON/OMCI driver answered the query; false otherwise.
 */
typedef bool (*nss_pon_query_t)(struct nss_pon_plugin_query_info *info, struct nss_pon_action *action);

/**
 * Data structure for PON/OMCI query operations.
 */
struct nss_pon_ops {
	nss_pon_query_t query;		/**< Query callback, handles both IPv4 and IPv6 flows. */
};

/**
 * Registers a client (the PON/OMCI driver) for the PON/OMCI query.
 *
 * @param	ops	The PON/OMCI query operations.
 *
 * @return
 * 0 if success, error value if fails.
 */
int nss_pon_plugin_register(struct nss_pon_ops *ops);

/**
 * Unregisters a client from the PON/OMCI query.
 *
 * @return
 * None.
 */
void nss_pon_plugin_unregister(void);

/**
 * Invokes the registered PON/OMCI query callback, if any.
 *
 * This is the only entry point acceleration engines (ECM) should use to
 * reach the PON/OMCI driver; it hides the RCU-protected storage owned by
 * this plugin module so that neither side needs to know how the callback
 * is stored.
 *
 * @param	info	The flow information gathered by the acceleration engine.
 * @param	action	The decision filled in by the PON/OMCI driver, if registered.
 *
 * @return
 * true if a PON/OMCI driver is registered and answered the query; false otherwise.
 */
bool nss_pon_plugin_query(struct nss_pon_plugin_query_info *info, struct nss_pon_action *action);

/**
 * Reports whether a PON/OMCI driver is currently registered.
 *
 * Acceleration engines can use this as a cheap pre-check to skip building
 * a struct nss_pon_plugin_query_info (interface hierarchy walks, etc.) when no
 * PON/OMCI driver is present at all.
 *
 * @return
 * true if a PON/OMCI driver is registered.
 */
bool nss_pon_plugin_is_registered(void);

/**
 * @}
 */

#endif /* __NSS_PON_PLUGIN_PUBLIC_H__ */
