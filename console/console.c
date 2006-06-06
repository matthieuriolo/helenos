/*
 * Copyright (C) 2006 Josef Cejka
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
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
/* TODO: remove */
#include <stdio.h>


#include <kbd.h>
#include <fb.h>
#include <ipc/ipc.h>
#include <ipc/fb.h>
#include <ipc/services.h>
#include <errno.h>
#include <key_buffer.h>
#include <console.h>
#include <unistd.h>
#include <async.h>
#include <libadt/fifo.h>
#include <screenbuffer.h>
#include <sys/mman.h>

#include "gcons.h"

#define MAX_KEYREQUESTS_BUFFERED 32

#define NAME "CONSOLE"

/** Index of currently used virtual console.
 */
int active_console = 0;

/** Information about framebuffer
 */
struct {
	int phone;		/**< Framebuffer phone */
	ipcarg_t rows;		/**< Framebuffer rows */
	ipcarg_t cols;		/**< Framebuffer columns */
} fb_info;


typedef struct {
	keybuffer_t keybuffer;		/**< Buffer for incoming keys. */
	FIFO_CREATE_STATIC(keyrequests, ipc_callid_t , MAX_KEYREQUESTS_BUFFERED);	/**< Buffer for unsatisfied request for keys. */
	int keyrequest_counter;		/**< Number of requests in buffer. */
	int client_phone;		/**< Phone to connected client. */
	int used;			/**< 1 if this virtual console is connected to some client.*/
	screenbuffer_t screenbuffer;	/**< Screenbuffer for saving screen contents and related settings. */
} connection_t;

static connection_t connections[CONSOLE_COUNT];	/**< Array of data for virtual consoles */
static keyfield_t *interbuffer = NULL;			/**< Pointer to memory shared with framebufer used for faster virt. console switching */

static int kernel_pixmap = -1;      /**< Number of fb pixmap, where kernel console is stored */


/** Find unused virtual console.
 *
 */
static int find_free_connection(void) 
{
	int i = 0;
	
	for (i=0; i < CONSOLE_COUNT; i++) {
		if (!connections[i].used)
			return i;
	}
	return -1;
}

static void clrscr(void)
{
	async_msg(fb_info.phone, FB_CLEAR, 0);
}

static void curs_visibility(int v)
{
	async_msg(fb_info.phone, FB_CURSOR_VISIBILITY, v); 
}

static void curs_goto(int row, int col)
{
	async_msg_2(fb_info.phone, FB_CURSOR_GOTO, row, col); 
	
}

static void set_style(style_t *style)
{
	async_msg_2(fb_info.phone, FB_SET_STYLE, style->fg_color, style->bg_color); 
}

static void set_style_col(int fgcolor, int bgcolor)
{
	async_msg_2(fb_info.phone, FB_SET_STYLE, fgcolor, bgcolor); 
}

static void prtchr(char c, int row, int col)
{
	async_msg_3(fb_info.phone, FB_PUTCHAR, c, row, col);
	
}

/** Check key and process special keys. 
 *
 * */
static void write_char(int console, char key)
{
	screenbuffer_t *scr = &(connections[console].screenbuffer);
	
	switch (key) {
		case '\n':
			scr->position_y += 1;
			scr->position_x =  0;
			break;
		case '\r':
			break;
		case '\t':
			scr->position_x += 8;
			scr->position_x -= scr->position_x % 8; 
			break;
		case '\b':
			if (scr->position_x == 0) 
				break;

			scr->position_x--;

			if (console == active_console)
				prtchr(' ', scr->position_y, scr->position_x);
	
			screenbuffer_putchar(scr, ' ');
			
			break;
		default:	
			if (console == active_console)
				prtchr(key, scr->position_y, scr->position_x);
	
			screenbuffer_putchar(scr, key);
			scr->position_x++;
	}
	
	scr->position_y += (scr->position_x >= scr->size_x);
	
	if (scr->position_y >= scr->size_y) {
		scr->position_y = scr->size_y - 1;
		screenbuffer_clear_line(scr, scr->top_line++);
		if (console == active_console)
			async_msg(fb_info.phone, FB_SCROLL, 1);
	}
	
	scr->position_x = scr->position_x % scr->size_x;
	
	if (console == active_console)
		curs_goto(scr->position_y, scr->position_x);
	
}

/** Save current screen to pixmap, draw old pixmap
 *
 * @param oldpixmap Old pixmap
 * @return ID of pixmap of current screen
 */
static int switch_screens(int oldpixmap)
{
	int newpmap;
       
	/* Save screen */
	newpmap = async_req(fb_info.phone, FB_VP2PIXMAP, 0, NULL);
	if (newpmap < 0)
		return -1;

	if (oldpixmap != -1) {
		/* Show old screen */
		async_msg_2(fb_info.phone, FB_VP_DRAW_PIXMAP, 0, oldpixmap);
		/* Drop old pixmap */
		async_msg(fb_info.phone, FB_DROP_PIXMAP, oldpixmap);
	}
	
	return newpmap;
}

/** Switch to new console */
static void change_console(int newcons)
{
	connection_t *conn;
	static int console_pixmap = -1;
	int i, j;
	keyfield_t *field;
	style_t *style;
	char c;

	if (newcons == active_console)
		return;

	if (newcons == KERNEL_CONSOLE) {
		if (active_console == KERNEL_CONSOLE)
			return;
		active_console = KERNEL_CONSOLE;
		curs_visibility(0);

		if (kernel_pixmap == -1) { 
			/* store/restore unsupported */
			set_style_col(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);
			clrscr();
		} else {
			gcons_in_kernel();
			console_pixmap = switch_screens(kernel_pixmap);
			kernel_pixmap = -1;
		}

		__SYSCALL0(SYS_DEBUG_ENABLE_CONSOLE);
		return;
	} 
	
	if (console_pixmap != -1) {
		kernel_pixmap = switch_screens(console_pixmap);
		console_pixmap = -1;
	}
	active_console = newcons;
	gcons_change_console(newcons);
	conn = &connections[active_console];

	set_style(&conn->screenbuffer.style);
	curs_goto(conn->screenbuffer.position_y, conn->screenbuffer.position_x);
	curs_visibility(0);
	if (interbuffer) {
		for (i = 0; i < conn->screenbuffer.size_x; i++)
			for (j = 0; j < conn->screenbuffer.size_y; j++) 
				interbuffer[i + j*conn->screenbuffer.size_x] = *get_field_at(&(conn->screenbuffer),i, j);
		/* This call can preempt, but we are already at the end */
		j = async_req_2(fb_info.phone, FB_DRAW_TEXT_DATA, 0, 0, NULL, NULL);		
	};
	
	if ((!interbuffer) || (j != 0)){
		set_style(&conn->screenbuffer.style);
		clrscr();
		style = &conn->screenbuffer.style;

		for (j = 0; j < conn->screenbuffer.size_y; j++) 
			for (i = 0; i < conn->screenbuffer.size_x; i++) {
				field = get_field_at(&(conn->screenbuffer),i, j);
				if (!style_same(*style, field->style))
					set_style(&field->style);
				style = &field->style;
				if ((field->character == ' ') && (style_same(field->style, conn->screenbuffer.style)))
					continue;

				prtchr(field->character, j, i);
			}
	}
	
	curs_visibility(conn->screenbuffer.is_cursor_visible);
}

/** Handler for keyboard */
static void keyboard_events(ipc_callid_t iid, ipc_call_t *icall)
{
	ipc_callid_t callid;
	ipc_call_t call;
	int retval;
	int c;
	connection_t *conn;
	
	/* Ignore parameters, the connection is alread opened */
	while (1) {
		callid = async_get_call(&call);
		switch (IPC_GET_METHOD(call)) {
		case IPC_M_PHONE_HUNGUP:
			ipc_answer_fast(callid,0,0,0);
			/* TODO: Handle hangup */
			return;
		case KBD_PUSHCHAR:
			/* got key from keyboard driver */
			
			retval = 0;
			c = IPC_GET_ARG1(call);
			/* switch to another virtual console */
			
			conn = &connections[active_console];
//			if ((c >= KBD_KEY_F1) && (c < KBD_KEY_F1 + CONSOLE_COUNT)) {
			if ((c >= 0x101) && (c < 0x101 + CONSOLE_COUNT)) {
				async_serialize_start();
				if (c == 0x112)
					change_console(KERNEL_CONSOLE);
				else
					change_console(c - 0x101);
				async_serialize_end();
				break;
			}
			
			/* if client is awaiting key, send it */
			if (conn->keyrequest_counter > 0) {		
				conn->keyrequest_counter--;
				ipc_answer_fast(fifo_pop(conn->keyrequests), 0, c, 0);
				break;
			}
			
			/*FIXME: else store key to its buffer */
			keybuffer_push(&conn->keybuffer, c);
			
			break;
		default:
			retval = ENOENT;
		}
		ipc_answer_fast(callid, retval, 0, 0);
	}
}

/** Default thread for new connections */
static void client_connection(ipc_callid_t iid, ipc_call_t *icall)
{
	ipc_callid_t callid;
	ipc_call_t call;
	int consnum;
	ipcarg_t arg1, arg2;
	connection_t *conn;

	if ((consnum = find_free_connection()) == -1) {
		ipc_answer_fast(iid,ELIMIT,0,0);
		return;
	}
	conn = &connections[consnum];
	conn->used = 1;
	
	async_serialize_start();
	gcons_notify_connect(consnum);
	conn->client_phone = IPC_GET_ARG3(call);
	screenbuffer_clear(&conn->screenbuffer);
	
	/* Accept the connection */
	ipc_answer_fast(iid,0,0,0);

	while (1) {
		async_serialize_end();
		callid = async_get_call(&call);
		async_serialize_start();

		arg1 = arg2 = 0;
		switch (IPC_GET_METHOD(call)) {
		case IPC_M_PHONE_HUNGUP:
			gcons_notify_disconnect(consnum);
			
			/* Answer all pending requests */
			while (conn->keyrequest_counter > 0) {		
				conn->keyrequest_counter--;
				ipc_answer_fast(fifo_pop(conn->keyrequests), ENOENT, 0, 0);
				break;
			}
			
			/* Commit hangup */
			ipc_answer_fast(callid, 0,0,0);
			async_serialize_end();
			return;
		case CONSOLE_PUTCHAR:
			write_char(consnum, IPC_GET_ARG1(call));
			gcons_notify_char(consnum);
			break;
		case CONSOLE_CLEAR:
			/* Send message to fb */
			if (consnum == active_console) {
				async_msg(fb_info.phone, FB_CLEAR, 0); 
			}
			
			screenbuffer_clear(&conn->screenbuffer);
			
			break;
		case CONSOLE_GOTO:
			
			screenbuffer_goto(&conn->screenbuffer, IPC_GET_ARG2(call), IPC_GET_ARG1(call));
			if (consnum == active_console)
				curs_goto(IPC_GET_ARG1(call),IPC_GET_ARG2(call));
			
			break;

		case CONSOLE_GETSIZE:
			arg1 = fb_info.rows;
			arg2 = fb_info.cols;
			break;
		case CONSOLE_FLUSH:
			async_req_2(fb_info.phone, FB_FLUSH, 0, 0, NULL, NULL);		
			break;
		case CONSOLE_SET_STYLE:
			
			arg1 = IPC_GET_ARG1(call);
			arg2 = IPC_GET_ARG2(call);
			screenbuffer_set_style(&conn->screenbuffer,arg1, arg2);
			if (consnum == active_console)
				set_style_col(arg1, arg2);
				
			break;
		case CONSOLE_CURSOR_VISIBILITY:
			arg1 = IPC_GET_ARG1(call);
			conn->screenbuffer.is_cursor_visible = arg1;
			if (consnum == active_console)
				curs_visibility(arg1);
			break;
		case CONSOLE_GETCHAR:
			if (keybuffer_empty(&conn->keybuffer)) {
				/* buffer is empty -> store request */
				if (conn->keyrequest_counter < MAX_KEYREQUESTS_BUFFERED) {		
					fifo_push(conn->keyrequests, callid);
					conn->keyrequest_counter++;
				} else {
					/* no key available and too many requests => fail */
					ipc_answer_fast(callid, ELIMIT, 0, 0);
				}
				continue;
			};
			keybuffer_pop(&conn->keybuffer, (int *)&arg1);
			
			break;
		}
		ipc_answer_fast(callid, 0, arg1, arg2);
	}
}

int main(int argc, char *argv[])
{
	ipcarg_t phonehash;
	int kbd_phone, fb_phone;
	ipcarg_t retval, arg1 = 0xdead, arg2 = 0xbeef;
	int i;

	async_set_client_connection(client_connection);
	
	/* Connect to keyboard driver */

	while ((kbd_phone = ipc_connect_me_to(PHONE_NS, SERVICE_KEYBOARD, 0)) < 0) {
		usleep(10000);
	};
	
	if (ipc_connect_to_me(kbd_phone, SERVICE_CONSOLE, 0, &phonehash) != 0) {
		return -1;
	};

	/* Connect to framebuffer driver */
	
	while ((fb_info.phone = ipc_connect_me_to(PHONE_NS, SERVICE_VIDEO, 0)) < 0) {
		usleep(10000);
	}
	
	/* Save old kernel screen */
	kernel_pixmap = switch_screens(-1);

	/* Initialize gcons */
	gcons_init(fb_info.phone);
	/* Synchronize, the gcons can have something in queue */
	async_req(fb_info.phone, FB_FLUSH, 0, NULL);

	
	ipc_call_sync_2(fb_info.phone, FB_GET_CSIZE, 0, 0, &(fb_info.rows), &(fb_info.cols)); 
	set_style_col(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);
	clrscr();
	
	/* Init virtual consoles */
	for (i = 0; i < CONSOLE_COUNT; i++) {
		connections[i].used = 0;
		keybuffer_init(&(connections[i].keybuffer));
		
		connections[i].keyrequests.head = connections[i].keyrequests.tail = 0;
		connections[i].keyrequests.items = MAX_KEYREQUESTS_BUFFERED;
		connections[i].keyrequest_counter = 0;
		
		if (screenbuffer_init(&(connections[i].screenbuffer), fb_info.cols, fb_info.rows ) == NULL) {
			/*FIXME: handle error */
			return -1;
		}
	}
	connections[KERNEL_CONSOLE].used = 1;
	
	if ((interbuffer = mmap(NULL, sizeof(keyfield_t) * fb_info.cols * fb_info.rows , PROTO_READ|PROTO_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, 0 ,0 )) != NULL) {
		if (ipc_call_sync_3(fb_info.phone, IPC_M_AS_AREA_SEND, (ipcarg_t)interbuffer, 0, AS_AREA_READ, NULL, NULL, NULL) != 0) {
			munmap(interbuffer, sizeof(keyfield_t) * fb_info.cols * fb_info.rows);
			interbuffer = NULL;
		}
	}

	async_new_connection(phonehash, 0, NULL, keyboard_events);
	
	curs_goto(0,0);
	curs_visibility(connections[active_console].screenbuffer.is_cursor_visible);

	/* Register at NS */
	if (ipc_connect_to_me(PHONE_NS, SERVICE_CONSOLE, 0, &phonehash) != 0) {
		return -1;
	};
	
	async_manager();

	return 0;	
}

