/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 *  Copyright (C) 2001 Cluster File Systems, Inc. <braam@clusterfs.com>
 *
 *   This file is part of Lustre, http://www.lustre.org.
 *
 *   Lustre is free software; you can redistribute it and/or
 *   modify it under the terms of version 2 of the GNU General Public
 *   License as published by the Free Software Foundation.
 *
 *   Lustre is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Lustre; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Basic library routines.
 *
 */

#ifndef __LIBCFS_LINUX_CFS_PRIM_H__
#define __LIBCFS_LINUX_CFS_PRIM_H__

#ifndef __LIBCFS_LIBCFS_H__
#error Do not #include this file directly. #include <libcfs/libcfs.h> instead
#endif

#ifdef __KERNEL__
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/timer.h>

#include <linux/miscdevice.h>
#include <libcfs/linux/portals_compat25.h>
#include <asm/div64.h>

#include <libcfs/linux/linux-time.h>

/*
 * Pseudo device register
 */
typedef struct miscdevice		cfs_psdev_t;
#define cfs_psdev_register(dev)		misc_register(dev)
#define cfs_psdev_deregister(dev)	misc_deregister(dev)

/*
 * Sysctl register
 */
typedef struct ctl_table		cfs_sysctl_table_t;
typedef struct ctl_table_header		cfs_sysctl_table_header_t;

#define register_cfs_sysctl_table(t, a)	register_sysctl_table(t, a)
#define unregister_cfs_sysctl_table(t)	unregister_sysctl_table(t, a)

/*
 * Proc file system APIs
 */
typedef read_proc_t                     cfs_read_proc_t;
typedef write_proc_t                    cfs_write_proc_t;
typedef struct proc_dir_entry           cfs_proc_dir_entry_t;
#define cfs_create_proc_entry(n, m, p)  create_proc_entry(n, m, p)
#define cfs_free_proc_entry(e)          free_proc_entry(e)
#define cfs_remove_proc_entry(n, e)     remove_proc_entry(n, e)

/*
 * Wait Queue
 */
typedef int                             cfs_task_state_t;

#define CFS_TASK_INTERRUPTIBLE          TASK_INTERRUPTIBLE
#define CFS_TASK_UNINT                  TASK_UNINTERRUPTIBLE

typedef wait_queue_t			cfs_waitlink_t;
typedef wait_queue_head_t		cfs_waitq_t;

#define cfs_waitq_init(w)		init_waitqueue_head(w)
#define cfs_waitlink_init(l)		init_waitqueue_entry(l, current)
#define cfs_waitq_add(w, l)	        add_wait_queue(w, l)
#define cfs_waitq_add_exclusive(w, l)	add_wait_queue_exclusive(w, l)
#define cfs_waitq_forward(l, w)         do {} while(0)
#define cfs_waitq_del(w, l)	        remove_wait_queue(w, l)
#define cfs_waitq_active(w)	        waitqueue_active(w)
#define cfs_waitq_signal(w)	        wake_up(w)
#define cfs_waitq_signal_nr(w,n)	wake_up_nr(w, n)
#define cfs_waitq_broadcast(w)	        wake_up_all(w)
#define cfs_waitq_wait(l, s)		do {set_current_state(s);schedule();} while(0)
#define cfs_waitq_timedwait(l, s, t)	do {set_current_state(s);schedule_timeout(t);} while(0)

/* Kernel thread */
typedef int (*cfs_thread_t)(void *);
#define cfs_kernel_thread(func, a, f)   kernel_thread(func, a, f)

/*
 * Task struct
 */
typedef struct task_struct              cfs_task_t;
#define cfs_current()                   current
#define CFS_DECL_JOURNAL_DATA           void *journal_info
#define CFS_PUSH_JOURNAL                do {    \
        journal_info = current->journal_info;   \
        current->journal_info = NULL;           \
        } while(0)
#define CFS_POP_JOURNAL                 do {    \
        current->journal_info = journal_info;   \
        } while(0)

/* Module interfaces */
#define cfs_module(name, version, init, fini) \
module_init(init);                            \
module_exit(fini)

/*
 * Signal
 */
typedef sigset_t cfs_sigset_t;
#define cfs_sigmask_lock(t, f)          SIGNAL_MASK_LOCK(t, f)
#define cfs_sigmask_unlock(t, f)        SIGNAL_MASK_UNLOCK(t, f)
#define cfs_recalc_sigpending(t)        RECALC_SIGPENDING
#define cfs_signal_pending(t)           signal_pending(t)
#define cfs_sigfillset(s)               sigfillset(s)

#define cfs_set_sig_blocked(t, b)       do { (t)->blocked = b; } while(0)
#define cfs_get_sig_blocked(t)          (&(t)->blocked)

/*
 * Timer
 */
typedef struct timer_list cfs_timer_t;
typedef  void (*timer_func_t)(unsigned long);

#define cfs_init_timer(t)       init_timer(t)

static inline void cfs_timer_init(cfs_timer_t *t, void (*func)(unsigned long), void *arg)
{
        init_timer(t);
        t->function = (timer_func_t)func;
        t->data = (unsigned long)arg;
}

static inline void cfs_timer_done(cfs_timer_t *t)
{
        return;
}

static inline void cfs_timer_arm(cfs_timer_t *t, cfs_time_t deadline)
{
        mod_timer(t, deadline);
}

static inline void cfs_timer_disarm(cfs_timer_t *t)
{
        del_timer(t);
}

static inline int  cfs_timer_is_armed(cfs_timer_t *t)
{
        return timer_pending(t);
}

static inline cfs_time_t cfs_timer_deadline(cfs_timer_t *t)
{
        return t->expires;
}


/* deschedule for a bit... */
static inline void cfs_pause(cfs_duration_t ticks)
{
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(ticks);
}

#else   /* !__KERNEL__ */

#include "../user-prim.h"

#endif /* __KERNEL__ */

#endif
