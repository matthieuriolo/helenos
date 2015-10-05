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

#include <adt/prodcons.h>
#include <assert.h>
#include <async.h>
#include <errno.h>
#include <ipc/services.h>
#include <ipc/taskman.h>
#include <loader/loader.h>
#include <macros.h>
#include <ns.h>
#include <stdio.h>
#include <stdlib.h>

#include "task.h"
#include "taskman.h"

//TODO move to appropriate header file
extern async_sess_t *session_primary;

typedef struct {
	link_t link;
	async_sess_t *sess;
} sess_ref_t;

static prodcons_t sess_queue;


/*
 * Static functions
 */
static void connect_to_loader(ipc_callid_t iid, ipc_call_t *icall)
{
	//TODO explain why we don't explicitly accept connection request
	/* Spawn a loader. */
	int rc = loader_spawn("loader");
	
	if (rc != EOK) {
		async_answer_0(iid, rc);
		return;
	}
	
	/* Wait until spawned task presents itself to us. */
	link_t *link = prodcons_consume(&sess_queue);
	sess_ref_t *sess_ref = list_get_instance(link, sess_ref_t, link);

	/* Forward the connection request (strip interface arg). */
	async_exch_t *exch = async_exchange_begin(sess_ref->sess);
	rc = async_forward_fast(iid, exch,
	    IPC_GET_ARG2(*icall),
	    IPC_GET_ARG3(*icall),
	    0, IPC_FF_NONE);
	async_exchange_end(exch);

	free(sess_ref);

	if (rc != EOK) {
		async_answer_0(iid, rc);
		return;
	}

	/* Everything OK. */
}

static void loader_to_ns(ipc_callid_t iid, ipc_call_t *icall)
{
	/* Do no accept connection request, forward it instead. */
	async_exch_t *exch = async_exchange_begin(session_primary);
	int rc = async_forward_fast(iid, exch, 0, 0, 0, IPC_FF_NONE);
	async_exchange_end(exch);

	if (rc != EOK) {
		async_answer_0(iid, rc);
		return;
	}
}

static void taskman_ctl_wait(ipc_callid_t iid, ipc_call_t *icall)
{
	task_id_t id = (task_id_t)
	    MERGE_LOUP32(IPC_GET_ARG1(*icall), IPC_GET_ARG2(*icall));
	int flags = IPC_GET_ARG3(*icall);

	wait_for_task(id, flags, iid, icall);
}

static void taskman_ctl_retval(ipc_callid_t iid, ipc_call_t *icall)
{
	printf("%s:%i\n", __func__, __LINE__);
	int rc = task_set_retval(icall);
	async_answer_0(iid, rc);
}

static void control_connection_loop(void)
{
	while (true) {
		ipc_call_t call;
		ipc_callid_t callid = async_get_call(&call);

		if (!IPC_GET_IMETHOD(call)) {
			/* Client disconnected */
			break;
		}

		switch (IPC_GET_IMETHOD(call)) {
		case TASKMAN_WAIT:
			taskman_ctl_wait(callid, &call);
			break;
		case TASKMAN_RETVAL:
			taskman_ctl_retval(callid, &call);
			break;
		default:
			async_answer_0(callid, ENOENT);
		}
	}
}

static void control_connection(ipc_callid_t iid, ipc_call_t *icall)
{
	/* First, accept connection */
	async_answer_0(iid, EOK);

	// TODO register task to hash table
	control_connection_loop();
}

static void loader_callback(ipc_callid_t iid, ipc_call_t *icall)
{
	// TODO check that loader is expected, would probably discard prodcons
	//      scheme
	
	/* Preallocate session container */
	sess_ref_t *sess_ref = malloc(sizeof(sess_ref_t));
	if (sess_ref == NULL) {
		async_answer_0(iid, ENOMEM);
	}
	

	/* Create callback connection */
	sess_ref->sess = async_callback_receive_start(EXCHANGE_ATOMIC, icall);
	if (sess_ref->sess == NULL) {
		//TODO different error code?
		async_answer_0(iid, EINVAL);
		return;
	}
	async_answer_0(iid, EOK);

	link_initialize(&sess_ref->link);
	prodcons_produce(&sess_queue, &sess_ref->link);
}

static void taskman_connection(ipc_callid_t iid, ipc_call_t *icall, void *arg)
{
	taskman_interface_t iface = IPC_GET_ARG1(*icall);
	switch (iface) {
	case TASKMAN_CONNECT_TO_LOADER:
		connect_to_loader(iid, icall);
		break;
	case TASKMAN_LOADER_TO_NS:
		loader_to_ns(iid, icall);
		break;
	case TASKMAN_CONTROL:
		control_connection(iid, icall);
		// ---- interrupt here ----
		//   implement control connection body (setup wait)
		// ------------------------
		break;
	default:
		/* Unknown interface */
		async_answer_0(iid, ENOENT);
	}
}

static void implicit_connection(ipc_callid_t iid, ipc_call_t *icall, void *arg)
{
	taskman_interface_t iface = IPC_GET_ARG1(*icall);
	switch (iface) {
	case TASKMAN_LOADER_CALLBACK:
		loader_callback(iid, icall);
		// TODO register task to hashtable
		control_connection_loop();
		break;
	default:
		/* Unknown interface on implicit connection */
		async_answer_0(iid, EHANGUP);
	}
}

/** Build hard coded configuration */


int main(int argc, char *argv[])
{
	printf(NAME ": HelenOS task manager\n");

	prodcons_initialize(&sess_queue);
	int rc = task_init();
	if (rc != EOK) {
		return rc;
	}

	/* We're service too */
	rc = service_register(SERVICE_TASKMAN);
	if (rc != EOK) {
		printf("Cannot register at naming service (%i).", rc);
		return rc;
	}

	/* Start sysman server */
	async_set_client_connection(taskman_connection);
	async_set_implicit_connection(implicit_connection);

	printf(NAME ": Accepting connections\n");
	//TODO task_retval(EOK);
	async_manager();

	/* not reached */
	return 0;
}
