/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */
#include <stdint.h>

#include <rte_mbuf.h>

#include "virtqueue.h"
#include "virtio_logs.h"
#include "virtio_pci.h"
#include "virtio_rxtx_simple.h"

/*
 * Two types of mbuf to be cleaned:
 * 1) mbuf that has been consumed by backend but not used by virtio.
 * 2) mbuf that hasn't been consued by backend.
 */
struct rte_mbuf *
virtqueue_detatch_unused(struct virtqueue *vq)
{
	struct rte_mbuf *cookie;
	int idx;

	if (vq != NULL)
		for (idx = 0; idx < vq->vq_nentries; idx++) {
			cookie = vq->vq_descx[idx].cookie;
			if (cookie != NULL) {
				vq->vq_descx[idx].cookie = NULL;
				return cookie;
			}
		}
	return NULL;
}

/* Flush the elements in the used ring. */
void
virtqueue_rxvq_flush(struct virtqueue *vq)
{
	struct virtnet_rx *rxq = &vq->rxq;
	struct virtio_hw *hw = vq->hw;
	struct vring_used_elem *uep;
	struct vq_desc_extra *dxp;
	uint16_t used_idx, desc_idx;
	uint16_t nb_used, i;

	nb_used = VIRTQUEUE_NUSED(vq);

	for (i = 0; i < nb_used; i++) {
		used_idx = vq->vq_used_cons_idx & (vq->vq_nentries - 1);
		uep = &vq->vq_ring.used->ring[used_idx];
		if (hw->use_simple_rx) {
			desc_idx = used_idx;
			rte_pktmbuf_free(vq->sw_ring[desc_idx]);
			vq->vq_free_cnt++;
		} else {
			desc_idx = (uint16_t)uep->id;
			dxp = &vq->vq_descx[desc_idx];
			if (dxp->cookie != NULL) {
				rte_pktmbuf_free(dxp->cookie);
				dxp->cookie = NULL;
			}
			vq_ring_free_chain(vq, desc_idx);
		}
		vq->vq_used_cons_idx++;
	}

	if (hw->use_simple_rx) {
		while (vq->vq_free_cnt >= RTE_VIRTIO_VPMD_RX_REARM_THRESH) {
			virtio_rxq_rearm_vec(rxq);
			if (virtqueue_kick_prepare(vq))
				virtqueue_notify(vq);
		}
	}
}
