/*
 * Copyright (C) 2013-2015 Kay Sievers
 * Copyright (C) 2013-2015 Greg Kroah-Hartman <gregkh@linuxfoundation.org>
 * Copyright (C) 2013-2015 Daniel Mack <daniel@zonque.org>
 * Copyright (C) 2013-2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright (C) 2013-2015 Linux Foundation
 *
 * kdbus is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 */

#ifndef __KDBUS_NODE_H
#define __KDBUS_NODE_H

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/lockdep.h>
#include <linux/mutex.h>
#include <linux/wait.h>

struct kdbus_node;

enum kdbus_node_type {
	KDBUS_NODE_DOMAIN,
	KDBUS_NODE_CONTROL,
	KDBUS_NODE_BUS,
	KDBUS_NODE_ENDPOINT,
	KDBUS_NODE_CONNECTION,
	KDBUS_NODE_N,
};

typedef void (*kdbus_node_free_t) (struct kdbus_node *node);
typedef void (*kdbus_node_release_t) (struct kdbus_node *node, bool was_active);

struct kdbus_node {
	struct mutex lock;
	atomic_t refcnt;
	atomic_t active;
	wait_queue_head_t waitq;

#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map dep_map;
#endif

	/* static members */
	unsigned int type;
	kdbus_node_free_t free_cb;
	kdbus_node_release_t release_cb;
	umode_t mode;
	kuid_t uid;
	kgid_t gid;

	/* valid once linked */
	char *name;
	unsigned int hash;
	unsigned int id;
	struct kdbus_node *parent; /* may be NULL */
	struct rb_node rb;

	/* dynamic list of children */
	struct rb_root children;
};

#define kdbus_node_from_rb(_node) rb_entry((_node), struct kdbus_node, rb)

extern struct ida kdbus_node_ida;

void kdbus_node_init(struct kdbus_node *node, unsigned int type);

int kdbus_node_link(struct kdbus_node *node, struct kdbus_node *parent,
		    const char *name);

struct kdbus_node *kdbus_node_ref(struct kdbus_node *node);
struct kdbus_node *kdbus_node_unref(struct kdbus_node *node);

bool kdbus_node_is_active(struct kdbus_node *node);
bool kdbus_node_is_deactivated(struct kdbus_node *node);
bool kdbus_node_activate(struct kdbus_node *node);
void kdbus_node_deactivate(struct kdbus_node *node);
void kdbus_node_drain(struct kdbus_node *node);

bool kdbus_node_acquire(struct kdbus_node *node);
void kdbus_node_release(struct kdbus_node *node);

struct kdbus_node *kdbus_node_find_child(struct kdbus_node *node,
					 const char *name);
struct kdbus_node *kdbus_node_find_closest(struct kdbus_node *node,
					   unsigned int hash);
struct kdbus_node *kdbus_node_next_child(struct kdbus_node *node,
					 struct kdbus_node *prev);

/**
 * kdbus_node_assert_held() - lockdep assertion for active reference
 * @node:	node that should be held active
 *
 * The concept of active-references is very similar to rw-locks. Hence, we
 * support lockdep integration to check that the caller holds an active
 * reference to a node. This call integrates with lockdep and throws an
 * exception if the current context does not own an active reference to the
 * passed node.
 */
static inline void kdbus_node_assert_held(struct kdbus_node *node)
{
	lockdep_assert_held(node);
}

/**
 * kdbus_assert_held() - lockdep assertion for active references
 * @_obj:		object that should be held active
 *
 * This is the same as kdbus_node_assert_held(), but can be called directly
 * on an object that embeds 'struct kdbus_node' as a member called 'node'.
 */
#define kdbus_assert_held(_obj) kdbus_node_assert_held(&(_obj)->node)

#endif
