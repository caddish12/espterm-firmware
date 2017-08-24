#include <esp8266.h>
#include <httpd.h>
#include <cgiwebsocket.h>

#include "cgi_sockets.h"
#include "uart_driver.h"
#include "screen.h"
#include "uart_buffer.h"

#define SOCK_BUF_LEN 1024
static char sock_buff[SOCK_BUF_LEN];

volatile bool notify_available = true;

static ETSTimer notifyTim;
static ETSTimer notifyTim2;

// we're trying to do a kind of mutex here, without the actual primitives
// this might glitch, very rarely.
// it's recommended to put some delay between setting labels and updating the screen.

static void ICACHE_FLASH_ATTR
notifyTimCb(void *arg) {
	void *data = NULL;
	int max_bl, total_bl;
	cgiWebsockMeasureBacklog(URL_WS_UPDATE, &max_bl, &total_bl);

	if (!notify_available || (max_bl > 2048)) { // do not send if we have anything significant backlogged
		// postpone a little
		os_timer_disarm(&notifyTim);
		os_timer_setfn(&notifyTim, notifyTimCb, NULL);
		os_timer_arm(&notifyTim, 5, 0);
		return;
	}
	notify_available = false;

	for (int i = 0; i < 20; i++) {
		httpd_cgi_state cont = screenSerializeToBuffer(sock_buff, SOCK_BUF_LEN, &data);
		int flg = 0;
		if (cont == HTTPD_CGI_MORE) flg |= WEBSOCK_FLAG_MORE;
		if (i > 0) flg |= WEBSOCK_FLAG_CONT;
		cgiWebsockBroadcast(URL_WS_UPDATE, sock_buff, (int) strlen(sock_buff), flg);
		if (cont == HTTPD_CGI_DONE) break;
	}

	// cleanup
	screenSerializeToBuffer(NULL, SOCK_BUF_LEN, &data);

	notify_available = true;
}

static void ICACHE_FLASH_ATTR
notifyLabelsTimCb(void *arg)
{
	if (!notify_available) {
		// postpone a little
		os_timer_disarm(&notifyTim2);
		os_timer_setfn(&notifyTim2, notifyLabelsTimCb, NULL);
		os_timer_arm(&notifyTim2, 1, 0);
		return;
	}
	notify_available = false;

	screenSerializeLabelsToBuffer(sock_buff, SOCK_BUF_LEN);
	cgiWebsockBroadcast(URL_WS_UPDATE, sock_buff, (int) strlen(sock_buff), 0);

	notify_available = true;
}

/** Beep */
void ICACHE_FLASH_ATTR
send_beep(void)
{
	// here's some potential for a race error with the other broadcast functions :C
	cgiWebsockBroadcast(URL_WS_UPDATE, "B", 1, 0);
}


/**
 * Broadcast screen state to sockets.
 * This is a callback for the Screen module,
 * called after each visible screen modification.
 */
void ICACHE_FLASH_ATTR screen_notifyChange(ScreenNotifyChangeTopic topic)
{
	// this is not the most ideal/cleanest implementation
	// PRs are welcome for a nicer update "queue" solution
	if (termconf->display_tout_ms == 0) termconf->display_tout_ms = SCR_DEF_DISPLAY_TOUT_MS;

	if (topic == CHANGE_LABELS) {
		// separate timer from content change timer, to avoid losing that update
		os_timer_disarm(&notifyTim2);
		os_timer_setfn(&notifyTim2, notifyLabelsTimCb, NULL);
		os_timer_arm(&notifyTim2, termconf->display_tout_ms, 0);
	}
	else if (topic == CHANGE_CONTENT) {
		// throttle delay
		os_timer_disarm(&notifyTim);
		os_timer_setfn(&notifyTim, notifyTimCb, NULL);
		os_timer_arm(&notifyTim, termconf->display_tout_ms, 0);
	}
}

/** Socket received a message */
void ICACHE_FLASH_ATTR updateSockRx(Websock *ws, char *data, int len, int flags)
{
	char buf[20];
	// Add terminator if missing (seems to randomly happen)
	data[len] = 0;

	// TODO re-implement those, use single byte markers and B2 encoding

	ws_dbg("Sock RX str: %s, len %d", data, len);

	if (strstarts(data, "STR:")) {
		// pass string verbatim
		UART_SendAsync(data+4, -1);
	}
	else if (strstarts(data, "BTN:")) {
		// send button as low ASCII value 1-9
		u8 btnNum = (u8) (data[4] - '0');
		if (btnNum > 0 && btnNum < 10) {
			UART_SendAsync((const char *) &btnNum, 1);
		}
	}
	else if (strstarts(data, "TAP:")) {
		// this comes in as 0-based

		int y=0, x=0;

		char *pc=data+4;
		char c;
		int phase=0;

		while((c=*pc++) != '\0') {
			if (c==','||c==';') {
				phase++;
			}
			else if (c>='0' && c<='9') {
				if (phase==0) {
					y=y*10+(c-'0');
				} else {
					x=x*10+(c-'0');
				}
			}
		}

		if (!screen_isCoordValid(y, x)) {
			ws_warn("Mouse input at invalid coordinates");
			return;
		}

		ws_dbg("Screen clicked at row %d, col %d", y+1, x+1);

		// Send as 1-based to user
		sprintf(buf, "\033[%d;%dM", y+1, x+1);
		UART_WriteString(UART0, buf, UART_TIMEOUT_US);
	}
	else {
		ws_warn("Bad command.");
	}
}

/** Socket connected for updates */
void ICACHE_FLASH_ATTR updateSockConnect(Websock *ws)
{
	ws_info("Socket connected to "URL_WS_UPDATE);
	ws->recvCb = updateSockRx;
}
