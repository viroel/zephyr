/** @file
 @brief TCP data handler

 This is not to be included by the application.
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TCP_H
#define __TCP_H

#include <zephyr/types.h>

#include <net/net_core.h>
#include <net/net_ip.h>
#include <net/net_pkt.h>
#include <net/net_context.h>

#include "connection.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Is this TCP context/socket used or not */
#define NET_TCP_IN_USE BIT(0)

/** Is the final segment sent */
#define NET_TCP_FINAL_SENT BIT(1)

/** Is the final segment received */
#define NET_TCP_FINAL_RECV BIT(2)

/** Is the socket shutdown for read/write */
#define NET_TCP_IS_SHUTDOWN BIT(3)

/** A retransmitted packet has been sent and not yet ack'd */
#define NET_TCP_RETRYING BIT(4)

/** MSS option has been set already */
#define NET_TCP_RECV_MSS_SET BIT(5)

/*
 * TCP connection states
 */
enum net_tcp_state {
	NET_TCP_CLOSED = 0,
	NET_TCP_LISTEN,
	NET_TCP_SYN_SENT,
	NET_TCP_SYN_RCVD,
	NET_TCP_ESTABLISHED,
	NET_TCP_CLOSE_WAIT,
	NET_TCP_LAST_ACK,
	NET_TCP_FIN_WAIT_1,
	NET_TCP_FIN_WAIT_2,
	NET_TCP_TIME_WAIT,
	NET_TCP_CLOSING,
};

/* TCP packet types */
#define NET_TCP_FIN 0x01
#define NET_TCP_SYN 0x02
#define NET_TCP_RST 0x04
#define NET_TCP_PSH 0x08
#define NET_TCP_ACK 0x10
#define NET_TCP_URG 0x20
#define NET_TCP_CTL 0x3f

#define NET_TCP_FLAGS(hdr) (hdr->flags & NET_TCP_CTL)

/* Length of TCP header, including options */
/* "offset": 4-bit field in high nibble, units of dwords */
#define NET_TCP_HDR_LEN(hdr) (4 * ((hdr)->offset >> 4))

/* RFC 1122 4.2.2.6 "If an MSS option is not received at connection
 * setup, TCP MUST assume a default send MSS of 536"
 */
#define NET_TCP_DEFAULT_MSS   536

/* TCP max window size */
#define NET_TCP_MAX_WIN   (4 * 1024)

/* Maximal value of the sequence number */
#define NET_TCP_MAX_SEQ   0xffffffff

#define NET_TCP_MAX_OPT_SIZE  8

/* TCP Option codes */
#define NET_TCP_END_OPT          0
#define NET_TCP_NOP_OPT          1
#define NET_TCP_MSS_OPT          2
#define NET_TCP_WINDOW_SCALE_OPT 3

/* TCP Option sizes */
#define NET_TCP_END_SIZE          1
#define NET_TCP_NOP_SIZE          1
#define NET_TCP_MSS_SIZE          4
#define NET_TCP_WINDOW_SCALE_SIZE 3

/** Parsed TCP option values for net_tcp_parse_opts()  */
struct net_tcp_options {
	u16_t mss;
};

/* Max received bytes to buffer internally */
#define NET_TCP_BUF_MAX_LEN 1280

/* Max segment lifetime, in seconds */
#define NET_TCP_MAX_SEG_LIFETIME 60

struct net_context;

struct net_tcp {
	/** Network context back pointer. */
	struct net_context *context;

	/** Cookie pointer passed to net_context_recv() */
	void *recv_user_data;

	/** ACK message timer */
	struct k_delayed_work ack_timer;

	/** Timer for doing active close in case the peer FIN is lost. */
	struct k_delayed_work fin_timer;

	/** Retransmit timer */
	struct k_delayed_work retry_timer;

	/** List pointer used for TCP retransmit buffering */
	sys_slist_t sent_list;

	/** Max acknowledgment. */
	u32_t recv_max_ack;

	/** Current sequence number. */
	u32_t send_seq;

	/** Acknowledgment number to send in next packet. */
	u32_t send_ack;

	/** Last ACK value sent */
	u32_t sent_ack;

	/** Current retransmit period */
	u32_t retry_timeout_shift : 5;
	/** Flags for the TCP */
	u32_t flags : 8;
	/** Current TCP state */
	u32_t state : 4;
	/* An outbound FIN packet has been sent */
	u32_t fin_sent : 1;
	/* An inbound FIN packet has been received */
	u32_t fin_rcvd : 1;
	/** Remaining bits in this u32_t */
	u32_t _padding : 13;

	/** Accept callback to be called when the connection has been
	 * established.
	 */
	net_tcp_accept_cb_t accept_cb;

	/**
	 * Semaphore to signal TCP connection completion
	 */
	struct k_sem connect_wait;

	/**
	 * Current TCP receive window for our side
	 */
	u16_t recv_wnd;

	/**
	 * Send MSS for the peer
	 */
	u16_t send_mss;
};

static inline bool net_tcp_is_used(struct net_tcp *tcp)
{
	NET_ASSERT(tcp);

	return tcp->flags & NET_TCP_IN_USE;
}

/**
 * @brief Register a callback to be called when TCP packet
 * is received corresponding to received packet.
 *
 * @param remote_addr Remote address of the connection end point.
 * @param local_addr Local address of the connection end point.
 * @param remote_port Remote port of the connection end point.
 * @param local_port Local port of the connection end point.
 * @param cb Callback to be called
 * @param user_data User data supplied by caller.
 * @param handle TCP handle that can be used when unregistering
 *
 * @return Return 0 if the registration succeed, <0 otherwise.
 */
static inline int net_tcp_register(const struct sockaddr *remote_addr,
				   const struct sockaddr *local_addr,
				   u16_t remote_port,
				   u16_t local_port,
				   net_conn_cb_t cb,
				   void *user_data,
				   struct net_conn_handle **handle)
{
	return net_conn_register(IPPROTO_TCP, remote_addr, local_addr,
				 remote_port, local_port, cb, user_data,
				 handle);
}

/**
 * @brief Unregister TCP handler.
 *
 * @param handle Handle from registering.
 *
 * @return Return 0 if the unregistration succeed, <0 otherwise.
 */
static inline int net_tcp_unregister(struct net_conn_handle *handle)
{
	return net_conn_unregister(handle);
}

/*
 * @brief Generate initial TCP sequence number
 *
 * @return Return a random TCP sequence number
 */
static inline u32_t tcp_init_isn(void)
{
	/* Randomise initial seq number */
	return sys_rand32_get();
}

const char *net_tcp_state_str(enum net_tcp_state state);

#if defined(CONFIG_NET_TCP)
void net_tcp_change_state(struct net_tcp *tcp, enum net_tcp_state new_state);
#else
#define net_tcp_change_state(...)
#endif

/**
 * @brief Allocate TCP connection context.
 *
 * @param context Pointer to net_context that is tied to this TCP
 * context.
 *
 * @return Pointer TCP connection context. NULL if no available
 * context can be found.
 */
struct net_tcp *net_tcp_alloc(struct net_context *context);

/**
 * @brief Release TCP connection context.
 *
 * @param tcp Pointer to net_tcp context.
 *
 * @return 0 if ok, < 0 if error
 */
int net_tcp_release(struct net_tcp *tcp);

/**
 * @brief Send a TCP segment without any data. The returned buffer
 * is a ready made packet that can be sent via net_send_data()
 * function.
 *
 * @param tcp TCP context
 * @param flags TCP flags
 * @param options Pointer TCP options, NULL if no options.
 * @param optlen Length of the options.
 * @param local Source address, or NULL to use the local address of
 *        the TCP context
 * @param remote Peer address
 * @param send_pkt Full IP + TCP header that is to be sent.
 *
 * @return 0 if ok, < 0 if error
 */
int net_tcp_prepare_segment(struct net_tcp *tcp, u8_t flags,
			    void *options, size_t optlen,
			    const struct sockaddr_ptr *local,
			    const struct sockaddr *remote,
			    struct net_pkt **send_pkt);

/**
 * @brief Prepare a TCP ACK message that can be send to peer.
 *
 * @param tcp TCP context
 * @param remote Peer address
 * @param pkt Network buffer
 *
 * @return 0 if ok, < 0 if error
 */
int net_tcp_prepare_ack(struct net_tcp *tcp, const struct sockaddr *remote,
			struct net_pkt **pkt);

/**
 * @brief Prepare a TCP RST message that can be send to peer.
 *
 * @param tcp TCP context
 * @param remote Peer address
 * @param pkt Network buffer
 *
 * @return 0 if ok, < 0 if error
 */
int net_tcp_prepare_reset(struct net_tcp *tcp, const struct sockaddr *remote,
			  struct net_pkt **pkt);

typedef void (*net_tcp_cb_t)(struct net_tcp *tcp, void *user_data);

/**
 * @brief Go through all the TCP connections and call callback
 * for each of them.
 *
 * @param cb User supplied callback function to call.
 * @param user_data User specified data.
 */
void net_tcp_foreach(net_tcp_cb_t cb, void *user_data);

/**
 * @brief Send available queued data over TCP connection
 *
 * @param context TCP context
 *
 * @return 0 if ok, < 0 if error
 */
int net_tcp_send_data(struct net_context *context);

/**
 * @brief Enqueue a single packet for transmission
 *
 * @param context TCP context
 * @param pkt Packet
 *
 * @return 0 if ok, < 0 if error
 */
int net_tcp_queue_data(struct net_context *context, struct net_pkt *pkt);

/**
 * @brief Sends one TCP packet initialized with the _prepare_*()
 *        family of functions.
 *
 * @param pkt Packet
 */
int net_tcp_send_pkt(struct net_pkt *pkt);

/**
 * @brief Handle a received TCP ACK
 *
 * @param cts Context
 * @param seq Received ACK sequence number
 */
void net_tcp_ack_received(struct net_context *ctx, u32_t ack);

/**
 * @brief Calculates and returns the MSS for a given TCP context
 *
 * @param tcp TCP context
 *
 * @return Maximum Segment Size
 */
u16_t net_tcp_get_recv_mss(const struct net_tcp *tcp);

/**
 * @brief Returns the receive window for a given TCP context
 *
 * @param tcp TCP context
 *
 * @return Current TCP receive window
 */
u32_t net_tcp_get_recv_wnd(const struct net_tcp *tcp);

/**
 * @brief Obtains the state for a TCP context
 *
 * @param tcp TCP context
 */
static inline enum net_tcp_state net_tcp_get_state(const struct net_tcp *tcp)
{
	return (enum net_tcp_state)tcp->state;
}

/**
 * @brief Check if the sequence number is valid i.e., it is inside the window.
 *
 * @param tcp TCP context
 * @param pkt Network packet
 *
 * @return true if network packet sequence number is valid, false otherwise
 */
bool net_tcp_validate_seq(struct net_tcp *tcp, struct net_pkt *pkt);

#if defined(CONFIG_NET_TCP)
/**
 * @brief Get TCP packet header data from net_pkt. The array values are in
 * network byte order and other values are in host byte order.
 * Note that you must access the TCP header values by the returned pointer,
 * the hdr parameter is just a placeholder for the header data and it might
 * not contain anything if the header fits properly in the first fragment of
 * the network packet.
 *
 * @param pkt Network packet
 * @param hdr Where to place the header if it does not fit in first fragment
 * of the network packet. This might not be pupulated if TCP header fits in
 * net_buf fragment.
 *
 * @return Return pointer to header or NULL if something went wrong.
 *         Always use the returned pointer to access the TCP header.
 */
struct net_tcp_hdr *net_tcp_get_hdr(struct net_pkt *pkt,
				    struct net_tcp_hdr *hdr);

/**
 * @brief Set TCP packet header data in net_pkt.
 *
 * @details  The values in the header must be in network byte order.
 * This function is normally called after a call to net_tcp_get_hdr().
 * The hdr parameter value should be the same that is returned by function
 * net_tcp_get_hdr() call. Note that if the TCP header fits in first net_pkt
 * fragment, then this function will not do anything as the returned value
 * was pointing directly to net_pkt.
 *
 * @param pkt Network packet
 * @param hdr Header data pointer that was returned by net_tcp_get_hdr().
 *
 * @return Return hdr or NULL if error
 */
struct net_tcp_hdr *net_tcp_set_hdr(struct net_pkt *pkt,
				    struct net_tcp_hdr *hdr);

/**
 * @brief Set TCP checksum in network packet.
 *
 * @param pkt Network packet
 * @param frag Fragment where to start calculating the offset.
 * Typically this is set to pkt->frags by the caller.
 *
 * @return Return the actual fragment where the checksum was written.
 */
struct net_buf *net_tcp_set_chksum(struct net_pkt *pkt, struct net_buf *frag);

/**
 * @brief Get TCP checksum from network packet.
 *
 * @param pkt Network packet
 * @param frag Fragment where to start calculating the offset.
 * Typically this is set to pkt->frags by the caller.
 *
 * @return Return the checksum in host byte order.
 */
u16_t net_tcp_get_chksum(struct net_pkt *pkt, struct net_buf *frag);

/**
 * @brief Parse TCP options from network packet.
 *
 * Parse TCP options, returning MSS value (as that the only one we
 * handle so far).
 *
 * @param pkt Network packet
 * @param opt_totlen Total length of options to parse
 * @param opts Pointer to TCP options structure. (Each option is updated
 * only if present, so the structure must be initialized with the default
 * values.)
 *
 * @return 0 if no error, <0 in case of error
 */
int net_tcp_parse_opts(struct net_pkt *pkt, int opt_totlen,
		       struct net_tcp_options *opts);

#else

static inline u16_t net_tcp_get_chksum(struct net_pkt *pkt,
				       struct net_buf *frag)
{
	ARG_UNUSED(pkt);
	ARG_UNUSED(frag);
	return 0;
}

static inline struct net_buf *net_tcp_set_chksum(struct net_pkt *pkt,
						 struct net_buf *frag)
{
	ARG_UNUSED(pkt);
	ARG_UNUSED(frag);
	return NULL;
}

static inline struct net_tcp_hdr *net_tcp_get_hdr(struct net_pkt *pkt,
						  struct net_tcp_hdr *hdr)
{
	ARG_UNUSED(pkt);
	ARG_UNUSED(hdr);
	return NULL;
}

static inline struct net_tcp_hdr *net_tcp_set_hdr(struct net_pkt *pkt,
						  struct net_tcp_hdr *hdr)
{
	ARG_UNUSED(pkt);
	ARG_UNUSED(hdr);
	return NULL;
}
#endif

#if defined(CONFIG_NET_TCP)
void net_tcp_init(void);
#else
#define net_tcp_init(...)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __TCP_H */
