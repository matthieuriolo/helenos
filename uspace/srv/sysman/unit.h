/*
 * Copyright (c) 2015 Michal Koutny
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Unit terminology and OOP based on systemd.
 */
#ifndef SYSMAN_UNIT_H
#define SYSMAN_UNIT_H

#include <adt/hash_table.h>
#include <adt/list.h>
#include <conf/configuration.h>
#include <conf/ini.h>
#include <conf/text_parse.h>
#include <fibril_synch.h>

typedef enum {
	UNIT_TYPE_INVALID = -1,
	UNIT_TARGET = 0,
	UNIT_MOUNT,
	UNIT_CONFIGURATION,
} unit_type_t;

typedef enum {
	STATE_EMBRYO = 0,
	STATE_STARTING,
	STATE_STARTED,
	STATE_STOPPED,
	STATE_FAILED
} unit_state_t;

typedef struct {
	ht_link_t units;

	unit_type_t type;
	char *name;

	unit_state_t state;
	fibril_mutex_t state_mtx;
	fibril_condvar_t state_cv;

	list_t dependencies;
	list_t dependants;
} unit_t;

typedef struct unit_vmt unit_vmt_t;
struct unit_vmt;

#include "unit_cfg.h"
#include "unit_mnt.h"
#include "unit_tgt.h"

#define DEFINE_CAST(NAME, TYPE, ENUM_TYPE)                           \
	static inline TYPE *CAST_##NAME(unit_t *u)                   \
	{                                                            \
		if (u->type == ENUM_TYPE)                            \
			return (TYPE *)u;                            \
		else                                                 \
			return NULL;                                 \
	}                                                            \

DEFINE_CAST(CFG, unit_cfg_t, UNIT_CONFIGURATION)
DEFINE_CAST(MNT, unit_mnt_t, UNIT_MOUNT)
DEFINE_CAST(TGT, unit_tgt_t, UNIT_TARGET)

struct unit_vmt {
	size_t size;

	void (*init)(unit_t *);

	void (*destroy)(unit_t *);

	int (*load)(unit_t *, ini_configuration_t *, text_parse_t *);

	int (*start)(unit_t *);
};

extern unit_vmt_t *unit_type_vmts[];

#define DEFINE_UNIT_VMT(PREFIX)                                      \
	unit_vmt_t PREFIX##_ops = {                                  \
		.size    = sizeof(PREFIX##_t),                       \
		.init    = &PREFIX##_init,                           \
		.load    = &PREFIX##_load,                           \
		.destroy = &PREFIX##_destroy,                        \
		.start   = &PREFIX##_start                           \
	};

#define UNIT_VMT(UNIT) unit_type_vmts[(UNIT)->type]

extern unit_t *unit_create(unit_type_t);
extern void unit_destroy(unit_t **);

// TODO add flags argument with explicit notification?
extern void unit_set_state(unit_t *, unit_state_t);

extern int unit_load(unit_t *, ini_configuration_t *, text_parse_t *);
extern int unit_start(unit_t *);

extern unit_type_t unit_type_name_to_type(const char *);

extern const char *unit_name(const unit_t *);

extern bool unit_parse_unit_list(const char *, void *, text_parse_t *, size_t);

#endif
