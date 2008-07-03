/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) 2003 Cluster File Systems, Inc.
 *
 * This code is issued under the GNU General Public License.
 * See the file COPYING in this distribution
 */

#ifndef LOV_INTERNAL_H
#define LOV_INTERNAL_H

#include <lustre/lustre_user.h>

struct lov_lock_handles {
        struct portals_handle   llh_handle;
        atomic_t                llh_refcount;
        int                     llh_stripe_count;
        struct lustre_handle    llh_handles[0];
};

struct lov_request {
        struct obd_info          rq_oi;
        struct lov_request_set  *rq_rqset;

        struct list_head         rq_link;

        int                      rq_idx;        /* index in lov->tgts array */
        int                      rq_stripe;     /* stripe number */
        int                      rq_complete;
        int                      rq_rc;
        int                      rq_buflen;     /* length of sub_md */

        obd_count                rq_oabufs;
        obd_count                rq_pgaidx;
};

struct lov_request_set {
        struct ldlm_enqueue_info*set_ei;
        struct obd_info         *set_oi;
        atomic_t                 set_refcount;
        struct obd_export       *set_exp;
        /* XXX: There is @set_exp already, however obd_statfs gets obd_device
           only. */
        struct obd_device       *set_obd;
        int                      set_count;
        int                      set_completes;
        int                      set_success;
        struct llog_cookie      *set_cookies;
        int                      set_cookie_sent;
        struct obd_trans_info   *set_oti;
        obd_count                set_oabufs;
        struct brw_page         *set_pga;
        struct lov_lock_handles *set_lockh;
        struct list_head         set_list;
};

#define LOV_AP_MAGIC 8200

struct lov_async_page {
        int                             lap_magic;
        int                             lap_stripe;
        obd_off                         lap_sub_offset;
        obd_id                          lap_loi_id;
        obd_gr                          lap_loi_gr;
        void                            *lap_sub_cookie;
        struct obd_async_page_ops       *lap_caller_ops;
        void                            *lap_caller_data;
};

#define LAP_FROM_COOKIE(c)                                                     \
        (LASSERT(((struct lov_async_page *)(c))->lap_magic == LOV_AP_MAGIC),   \
         (struct lov_async_page *)(c))

extern cfs_mem_cache_t *lov_oinfo_slab;

static inline void lov_llh_addref(void *llhp)
{
        struct lov_lock_handles *llh = llhp;
        atomic_inc(&llh->llh_refcount);
        CDEBUG(D_INFO, "GETting llh %p : new refcount %d\n", llh,
               atomic_read(&llh->llh_refcount));
}

static inline struct lov_lock_handles *lov_llh_new(struct lov_stripe_md *lsm)
{
        struct lov_lock_handles *llh;

        OBD_ALLOC(llh, sizeof *llh +
                  sizeof(*llh->llh_handles) * lsm->lsm_stripe_count);
        if (llh == NULL)
                return NULL;
        atomic_set(&llh->llh_refcount, 2);
        llh->llh_stripe_count = lsm->lsm_stripe_count;
        CFS_INIT_LIST_HEAD(&llh->llh_handle.h_link);
        class_handle_hash(&llh->llh_handle, lov_llh_addref);
        return llh;
}

static inline struct lov_lock_handles *
lov_handle2llh(struct lustre_handle *handle)
{
        LASSERT(handle != NULL);
        return(class_handle2object(handle->cookie));
}

static inline void lov_llh_put(struct lov_lock_handles *llh)
{
        CDEBUG(D_INFO, "PUTting llh %p : new refcount %d\n", llh,
               atomic_read(&llh->llh_refcount) - 1);
        LASSERT(atomic_read(&llh->llh_refcount) > 0 &&
                atomic_read(&llh->llh_refcount) < 0x5a5a);
        if (atomic_dec_and_test(&llh->llh_refcount)) {
                class_handle_unhash(&llh->llh_handle);
                /* The structure may be held by other threads because RCU. 
                 *   -jxiong */
                if (atomic_read(&llh->llh_refcount))
                        return;

                OBD_FREE_RCU(llh, sizeof *llh +
                             sizeof(*llh->llh_handles) * llh->llh_stripe_count,
                             &llh->llh_handle);
        }
}

#define lov_uuid2str(lv, index) \
        (char *)((lv)->lov_tgts[index]->ltd_uuid.uuid)

/* lov_merge.c */
void lov_merge_attrs(struct obdo *tgt, struct obdo *src, obd_flag valid,
                     struct lov_stripe_md *lsm, int stripeno, int *set);
int lov_merge_lvb(struct obd_export *exp, struct lov_stripe_md *lsm,
                  struct ost_lvb *lvb, int kms_only);
int lov_adjust_kms(struct obd_export *exp, struct lov_stripe_md *lsm,
                   obd_off size, int shrink);

/* lov_offset.c */
obd_size lov_stripe_size(struct lov_stripe_md *lsm, obd_size ost_size,
                         int stripeno);
int lov_stripe_offset(struct lov_stripe_md *lsm, obd_off lov_off,
                      int stripeno, obd_off *obd_off);
obd_off lov_size_to_stripe(struct lov_stripe_md *lsm, obd_off file_size,
                           int stripeno);
int lov_stripe_intersects(struct lov_stripe_md *lsm, int stripeno,
                          obd_off start, obd_off end,
                          obd_off *obd_start, obd_off *obd_end);
int lov_stripe_number(struct lov_stripe_md *lsm, obd_off lov_off);

/* lov_qos.c */
#define LOV_USES_ASSIGNED_STRIPE        0
#define LOV_USES_DEFAULT_STRIPE         1
int qos_add_tgt(struct obd_device *obd, __u32 index);
int qos_del_tgt(struct obd_device *obd, __u32 index);
void qos_shrink_lsm(struct lov_request_set *set);
int qos_prep_create(struct obd_export *exp, struct lov_request_set *set);
void qos_update(struct lov_obd *lov);
int qos_remedy_create(struct lov_request_set *set, struct lov_request *req);

/* lov_request.c */
void lov_set_add_req(struct lov_request *req, struct lov_request_set *set);
void lov_update_set(struct lov_request_set *set,
                    struct lov_request *req, int rc);
int lov_update_common_set(struct lov_request_set *set,
                          struct lov_request *req, int rc);
int lov_prep_create_set(struct obd_export *exp, struct obd_info *oifo,
                        struct lov_stripe_md **ea, struct obdo *src_oa,
                        struct obd_trans_info *oti,
                        struct lov_request_set **reqset);
int lov_update_create_set(struct lov_request_set *set,
                          struct lov_request *req, int rc);
int lov_fini_create_set(struct lov_request_set *set, struct lov_stripe_md **ea);
int lov_prep_brw_set(struct obd_export *exp, struct obd_info *oinfo,
                     obd_count oa_bufs, struct brw_page *pga,
                     struct obd_trans_info *oti,
                     struct lov_request_set **reqset);
int lov_fini_brw_set(struct lov_request_set *set);
int lov_prep_getattr_set(struct obd_export *exp, struct obd_info *oinfo,
                         struct lov_request_set **reqset);
int lov_fini_getattr_set(struct lov_request_set *set);
int lov_prep_destroy_set(struct obd_export *exp, struct obd_info *oinfo,
                         struct obdo *src_oa, struct lov_stripe_md *lsm,
                         struct obd_trans_info *oti,
                         struct lov_request_set **reqset);
int lov_update_destroy_set(struct lov_request_set *set,
                           struct lov_request *req, int rc);
int lov_fini_destroy_set(struct lov_request_set *set);
int lov_prep_setattr_set(struct obd_export *exp, struct obd_info *oinfo,
                         struct obd_trans_info *oti,
                         struct lov_request_set **reqset);
int lov_update_setattr_set(struct lov_request_set *set,
                           struct lov_request *req, int rc);
int lov_fini_setattr_set(struct lov_request_set *set);
int lov_prep_punch_set(struct obd_export *exp, struct obd_info *oinfo,
                       struct obd_trans_info *oti,
                       struct lov_request_set **reqset);
int lov_fini_punch_set(struct lov_request_set *set);
int lov_prep_sync_set(struct obd_export *exp, struct obd_info *obd_info,
                      struct obdo *src_oa,
                      struct lov_stripe_md *lsm, obd_off start,
                      obd_off end, struct lov_request_set **reqset);
int lov_fini_sync_set(struct lov_request_set *set);
int lov_prep_enqueue_set(struct obd_export *exp, struct obd_info *oinfo,
                         struct ldlm_enqueue_info *einfo,
                         struct lov_request_set **reqset);
int lov_fini_enqueue_set(struct lov_request_set *set, __u32 mode, int rc,
                         struct ptlrpc_request_set *rqset);
int lov_prep_match_set(struct obd_export *exp, struct obd_info *oinfo,
                       struct lov_stripe_md *lsm,
                       ldlm_policy_data_t *policy, __u32 mode,
                       struct lustre_handle *lockh,
                       struct lov_request_set **reqset);
int lov_update_match_set(struct lov_request_set *set, struct lov_request *req,
                         int rc);
int lov_fini_match_set(struct lov_request_set *set, __u32 mode, int flags);
int lov_prep_cancel_set(struct obd_export *exp, struct obd_info *oinfo,
                        struct lov_stripe_md *lsm,
                        __u32 mode, struct lustre_handle *lockh,
                        struct lov_request_set **reqset);
int lov_fini_cancel_set(struct lov_request_set *set);
int lov_prep_statfs_set(struct obd_device *obd, struct obd_info *oinfo,
                        struct lov_request_set **reqset);
void lov_update_statfs(struct obd_device *obd, struct obd_statfs *osfs,
                       struct obd_statfs *lov_sfs, int success);
int lov_fini_statfs(struct obd_device *obd, struct obd_statfs *osfs,
                    int success);
int lov_fini_statfs_set(struct lov_request_set *set);

/* lov_obd.c */
void lov_fix_desc(struct lov_desc *desc);
void lov_fix_desc_stripe_size(__u64 *val);
void lov_fix_desc_stripe_count(__u32 *val);
void lov_fix_desc_pattern(__u32 *val);
void lov_fix_desc_qos_maxage(__u32 *val);
int lov_get_stripecnt(struct lov_obd *lov, __u32 stripe_count);
void lov_getref(struct obd_device *obd);
void lov_putref(struct obd_device *obd);

/* lov_log.c */
int lov_llog_init(struct obd_device *obd, struct obd_llog_group *olg,
                  struct obd_device *tgt, int count, struct llog_catid *logid, 
                  struct obd_uuid *uuid);
int lov_llog_finish(struct obd_device *obd, int count);

/* lov_pack.c */
int lov_packmd(struct obd_export *exp, struct lov_mds_md **lmm,
               struct lov_stripe_md *lsm);
int lov_unpackmd(struct obd_export *exp, struct lov_stripe_md **lsmp,
                 struct lov_mds_md *lmm, int lmm_bytes);
int lov_setstripe(struct obd_export *exp,
                  struct lov_stripe_md **lsmp, struct lov_user_md *lump);
int lov_setea(struct obd_export *exp, struct lov_stripe_md **lsmp,
              struct lov_user_md *lump);
int lov_getstripe(struct obd_export *exp,
                  struct lov_stripe_md *lsm, struct lov_user_md *lump);
int lov_alloc_memmd(struct lov_stripe_md **lsmp, int stripe_count,
                      int pattern, int magic);
void lov_free_memmd(struct lov_stripe_md **lsmp);

void lov_dump_lmm_v1(int level, struct lov_mds_md_v1 *lmm);
void lov_dump_lmm_join(int level, struct lov_mds_md_join *lmmj);
/* lov_ea.c */
int lov_unpackmd_join(struct lov_obd *lov, struct lov_stripe_md *lsm,
                      struct lov_mds_md *lmm);
struct lov_stripe_md *lsm_alloc_plain(int stripe_count, int *size);
void lsm_free_plain(struct lov_stripe_md *lsm);

struct lov_extent *lovea_idx2le(struct lov_stripe_md *lsm, int stripe_no);
struct lov_extent *lovea_off2le(struct lov_stripe_md *lsm, obd_off lov_off);
int lovea_destroy_object(struct lov_obd *lov, struct lov_stripe_md *lsm,
                         struct obdo *oa, void *data);
/* lproc_lov.c */
extern struct file_operations lov_proc_target_fops;
#ifdef LPROCFS
void lprocfs_lov_init_vars(struct lprocfs_static_vars *lvars);
#else
static inline void lprocfs_lov_init_vars(struct lprocfs_static_vars *lvars)
{
        memset(lvars, 0, sizeof(*lvars));
}
#endif

#endif
