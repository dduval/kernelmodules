#include <linux/types.h>
#include <linux/module.h>       /* for modules */
#include <linux/fs.h>           /* file_operations */
#include <asm/uaccess.h>        /* copy_(to,from)_user */
#include <linux/init.h>         /* module_init, module_exit */
#include <linux/slab.h>         /* kmalloc */
#include <net/icmp.h>
#include <net/tcp.h>
#include <net/inet_common.h>
#include <linux/inet.h>
#include <linux/stddef.h>
#include <linux/tcp_diag.h>


#define tcp_ehash (tcp_hashinfo.__tcp_ehash)
#define tcp_ehash_size  (tcp_hashinfo.__tcp_ehash_size)

#define MYDEV_NAME "cwndmon"
#define PRBUFSZ 1000

static struct tcp_iter_state global_st = {
        .family         = AF_INET,
};

/* Format the tcp_info structure nicely into a string of characters */
void print_tcp_info(char *printer_buffer, struct tcp_info *info)
{
	
        memset(printer_buffer, 0, PRBUFSZ);

	snprintf(printer_buffer,PRBUFSZ,"%d, %d, %d, %d,  \
	%d, %d, %d, %d, \
	%d, %d, %d, %d, \
	%d, %d, %d, %d, \
	%d, %d, %d, %d, \
	%d, %d, cwnd=%d, %d, \ 
	%d, %d, %d, %s, %d, %d, \
	%s, %s, %s", info->tcpi_state,
	info->tcpi_ca_state,
	info->tcpi_retransmits,
	info->tcpi_probes,

	info->tcpi_backoff,
        info->tcpi_rto,
        info->tcpi_ato,
        info->tcpi_snd_mss,

        info->tcpi_rcv_mss,
        info->tcpi_unacked,
        info->tcpi_sacked,
        info->tcpi_lost,

        info->tcpi_retrans,
        info->tcpi_fackets,
        info->tcpi_last_data_sent,
        info->tcpi_last_data_recv,

        info->tcpi_last_ack_recv,
        info->tcpi_pmtu,
        info->tcpi_rcv_ssthresh,
        info->tcpi_rtt,

        info->tcpi_rttvar,
        info->tcpi_snd_ssthresh,
        info->tcpi_snd_cwnd,
        info->tcpi_advmss,

        info->tcpi_reordering,
        info->tcpi_rcv_rtt,
        info->tcpi_rcv_space,
	(info->tcpi_options&TCPI_OPT_WSCALE) ? "WSCALE" : "",
        info->tcpi_snd_wscale,
        info->tcpi_rcv_wscale,

	(info->tcpi_options&TCPI_OPT_SACK) ? "SACK" : "",
	(info->tcpi_options&TCPI_OPT_TIMESTAMPS) ? "TIMESTAMPS" : "",
	(info->tcpi_options&TCPI_OPT_ECN) ? "ECN" : ""
	
	);


}



/* Return information about state of tcp endpoint in API format. */
void tcp_get_info(struct sock *sk, struct tcp_info *info)
{
        struct tcp_opt *tp = tcp_sk(sk);
        u32 now = tcp_time_stamp;

        memset(info, 0, sizeof(*info));

        info->tcpi_state = sk->sk_state;
        info->tcpi_ca_state = tp->ca_state;
        info->tcpi_retransmits = tp->retransmits;
        info->tcpi_probes = tp->probes_out;
        info->tcpi_backoff = tp->backoff;

        if (tp->tstamp_ok)
                info->tcpi_options |= TCPI_OPT_TIMESTAMPS;
        if (tp->sack_ok)
                info->tcpi_options |= TCPI_OPT_SACK;
        if (tp->wscale_ok) {
                info->tcpi_options |= TCPI_OPT_WSCALE;
                info->tcpi_snd_wscale = tp->snd_wscale;
                info->tcpi_rcv_wscale = tp->rcv_wscale;
        }


        if (tp->ecn_flags&TCP_ECN_OK)
                info->tcpi_options |= TCPI_OPT_ECN;

        info->tcpi_rto = jiffies_to_usecs(tp->rto);
        info->tcpi_ato = jiffies_to_usecs(tp->ack.ato);
        info->tcpi_snd_mss = tp->mss_cache_std;
        info->tcpi_rcv_mss = tp->ack.rcv_mss;

        info->tcpi_unacked = tcp_get_pcount(&tp->packets_out);
        info->tcpi_sacked = tcp_get_pcount(&tp->sacked_out);
        info->tcpi_lost = tcp_get_pcount(&tp->lost_out);
        info->tcpi_retrans = tcp_get_pcount(&tp->retrans_out);
        info->tcpi_fackets = tcp_get_pcount(&tp->fackets_out);

        info->tcpi_last_data_sent = jiffies_to_msecs(now - tp->lsndtime);
        info->tcpi_last_data_recv = jiffies_to_msecs(now - tp->ack.lrcvtime);
        info->tcpi_last_ack_recv = jiffies_to_msecs(now - tp->rcv_tstamp);

        info->tcpi_pmtu = tp->pmtu_cookie;
        info->tcpi_rcv_ssthresh = tp->rcv_ssthresh;
        info->tcpi_rtt = jiffies_to_usecs(tp->srtt)>>3;
        info->tcpi_rttvar = jiffies_to_usecs(tp->mdev)>>2;
        info->tcpi_snd_ssthresh = tp->snd_ssthresh;
        info->tcpi_snd_cwnd = tp->snd_cwnd;
        info->tcpi_advmss = tp->advmss;
        info->tcpi_reordering = tp->reordering;

        info->tcpi_rcv_rtt = jiffies_to_usecs(tp->rcv_rtt_est.rtt)>>3;
        info->tcpi_rcv_space = tp->rcvq_space.space;
}


static inline struct tcp_tw_bucket *tw_head(struct hlist_head *head)
{
        return hlist_empty(head) ? NULL :
                list_entry(head->first, struct tcp_tw_bucket, tw_node);
}


static inline struct tcp_tw_bucket *tw_next(struct tcp_tw_bucket *tw)
{
        return tw->tw_node.next ?
                hlist_entry(tw->tw_node.next, typeof(*tw), tw_node) : NULL;
}

static void *established_get_first(void)
{
        void *rc = NULL;
	struct tcp_iter_state* st = &global_st;

        for (st->bucket = 0; st->bucket < tcp_ehash_size; ++st->bucket) {
                struct sock *sk;
                struct hlist_node *node;
                struct tcp_tw_bucket *tw;

                read_lock(&tcp_ehash[st->bucket].lock);
                sk_for_each(sk, node, &tcp_ehash[st->bucket].chain) {
                        if (sk->sk_family != st->family) {
                                continue;
                        }
                        rc = sk;
                        goto out;
                }
                st->state = TCP_SEQ_STATE_TIME_WAIT;
                tw_for_each(tw, node, &tcp_ehash[st->bucket + tcp_ehash_size].chain) {
                        if (tw->tw_family != st->family) {
                                continue;
                        }
                        rc = tw;
                        goto out;
                }
                read_unlock(&tcp_ehash[st->bucket].lock);
                st->state = TCP_SEQ_STATE_ESTABLISHED;
        }
out:
        return rc;
}

static void *established_get_next(void *cur)
{
        struct sock *sk = cur;
        struct tcp_tw_bucket *tw;
        struct hlist_node *node;
        struct tcp_iter_state* st = &global_st;

        ++st->num;

        if (st->state == TCP_SEQ_STATE_TIME_WAIT) {
                tw = cur;
                tw = tw_next(tw);
get_tw:
                while (tw && tw->tw_family != st->family) {
                        tw = tw_next(tw);
                }
                if (tw) {
                        cur = tw;
                        goto out;
                }
                read_unlock(&tcp_ehash[st->bucket].lock);
                st->state = TCP_SEQ_STATE_ESTABLISHED;
                if (++st->bucket < tcp_ehash_size) {
                        read_lock(&tcp_ehash[st->bucket].lock);
                        sk = sk_head(&tcp_ehash[st->bucket].chain);
                } else {
                        cur = NULL;
                        goto out;
                }
        } else
                sk = sk_next(sk);

        sk_for_each_from(sk, node) {
                if (sk->sk_family == st->family)
                        goto found;
        }

        st->state = TCP_SEQ_STATE_TIME_WAIT;
        tw = tw_head(&tcp_ehash[st->bucket + tcp_ehash_size].chain);
        goto get_tw;
found:
        cur = sk;
out:
        return cur;
}



static int __init my_init (void)
{

	void *rc = established_get_first();
	struct tcp_info info;
	char *printer_buffer;

	printer_buffer = kmalloc(PRBUFSZ,GFP_KERNEL);
	
        while (rc) {
		tcp_get_info(rc, &info);
		print_tcp_info(printer_buffer, &info);
		printk("PID: %d, %s\n",((struct sock *)rc)->sk_peercred.pid, printer_buffer);
                rc = established_get_next(rc);
        }

   	return -1;
}



static void __exit my_exit (void)
{
}

module_init (my_init);
module_exit (my_exit);

MODULE_LICENSE("GPL");
