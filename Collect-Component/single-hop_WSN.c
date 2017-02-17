/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Best-effort single-hop unicast example
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "net/rime/rime.h"
#include "lib/error.h"
#include "lib/list.h"
#include "lib/random.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include <stdio.h>
#include <stdlib.h>

#include "../apps/param-repo/param-repo.h"

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...) do {} while (0)
#endif

typedef struct app_header {
	uint16_t seq; //16 bit seq number
	uint32_t ts; //32 bit timestamp in usec
} app_header_t;

typedef struct app_data {
	app_header_t hdr;
	const char payload[100];
} app_data_t;

uint32_t current_send_interval = 0;
uint32_t send_interval = 128;
linkaddr_t receiver = {{1,0}};
uint8_t applicationActive = 0;
uint8_t msg_size = 100;
uint8_t num_senders = 0;
uint32_t start_time_us;
app_data_t msg = {{0,0},{"single-hop_WSN:1234567812345678123456781234567812345678123456781234567812345678123456:single-hop_WSN"}};

#define MAX_PEERS 100

struct tx_stats_t {
	uint32_t latency_sum;
	uint16_t latency_count;
};
struct rx_stats_t {
	int16_t rssi_sum;
	uint16_t lqi_sum;
	uint16_t packet_recvd;
	uint16_t packet_lost;
	uint16_t seq_nr;
};

struct tx_stats_t tx_stats = {0};
struct rx_stats_t *rx_stats = NULL;
linkaddr_t source_IDs[MAX_PEERS];
uint8_t source_len;

static struct unicast_conn uc;
linkaddr_t uc_from;


/*---------------------------------------------------------------------------*/
PROCESS(example_unicast_process, "Example unicast");
PROCESS(uart_process, "UART WRITING EVENTS PROCESS");
AUTOSTART_PROCESSES(&example_unicast_process);
/*---------------------------------------------------------------------------*/

enum {
	RIME_EXAMPLEUNICASTSENDINTERVAL	= 19367,
	RIME_EXAMPLEUNICASTACTIVATEAPP	= 25440,
	RIME_EXAMPLEUNICASTMSGSIZE	= 57215,
	RIME_EXAMPLEUNICASTRECEIVER	= 57217,
	RIME_EXAMPLEUNICASTSOURCEIDS	= 57218,
	RIME_EXAMPLEUNICASTTXSTATS	= 57219,
	RIME_EXAMPLEUNICASTRXSTATS	= 57216
};

static error_t activateAPP(void* value, control_hdr_t* p){
	uint8_t temp = applicationActive;
	PRINTF("single-hop_WSN activateAPP ");
	memcpy(&applicationActive,value,p->len);
	if(temp != applicationActive){
		if(applicationActive){
			PRINTF( "(START): ");
			process_start(&example_unicast_process,&msg);
		} else {
			PRINTF( "(STOP): ");
			process_exit(&example_unicast_process);
		}
		PRINTF("SUCCESS\n");
		return SUCCESS;
	}
	PRINTF("EALREADY\n");
	return EALREADY;
}

static void* getParameter(control_hdr_t* p){
	if(p->uid == RIME_EXAMPLEUNICASTSENDINTERVAL){
		return &send_interval;
	} else if(p->uid == RIME_EXAMPLEUNICASTACTIVATEAPP){
		return &applicationActive;
	} else if(p->uid == RIME_EXAMPLEUNICASTMSGSIZE){
		return &msg_size;
	} else if(p->uid == RIME_EXAMPLEUNICASTRECEIVER){
		return &receiver;
	} else if(p->uid == RIME_EXAMPLEUNICASTSOURCEIDS){
		return &source_IDs;
	}else if(p->uid == RIME_EXAMPLEUNICASTTXSTATS){
//		printf("tx_stats GET source=%u latency_sum=%lu latency_count=%u\n", uc_from, tx_stats.latency_sum, tx_stats.latency_count);
		return &tx_stats;
	}else if(p->uid == RIME_EXAMPLEUNICASTRXSTATS){
		// sender ID is sent through type parameter
		memcpy(&uc_from, &(p->type), sizeof(uint8_t));

		// Search for sender ID
		int i;
		for(i = 0; i < source_len; i++)
		{
			if(linkaddr_cmp(&(source_IDs[i]), &uc_from))
			{
//				printf("GET source=%u rssi_sum=%d lqi_sum=%u packet_recvd=%u packet_lost=%u seq_nr=%u\n", uc_from, rx_stats[i].rssi_sum, rx_stats[i].lqi_sum, rx_stats[i].packet_recvd, rx_stats[i].packet_lost, rx_stats[i].seq_nr);

				// Get RxStat values
				return &(rx_stats[i]);
			}
		}
	}
	return NULL;
}

static error_t setParameter(void* value, control_hdr_t* p){
	PRINTF("single-hop_WSN SET PARAMETER %u:",p->uid);
	if(p->uid == RIME_EXAMPLEUNICASTSENDINTERVAL){
		memcpy(&send_interval, value, sizeof(uint32_t));
		PRINTF(" send_interval %lu \n",send_interval);
		return SUCCESS;
	} else if(p->uid == RIME_EXAMPLEUNICASTMSGSIZE){
		memcpy(&msg_size,value,p->len);
		PRINTF("msg_size %u\n",msg_size);
		return SUCCESS;
	}else if(p->uid == RIME_EXAMPLEUNICASTRECEIVER){
		memcpy(&receiver,value,p->len);
		PRINTF("receiver %u\n",(*((uint16_t*)value)));
		return SUCCESS;
	}else if(p->uid == RIME_EXAMPLEUNICASTSOURCEIDS){
		memcpy(source_IDs, value, p->len);
		source_len = (int)(p->len/2);

		rx_stats = (struct rx_stats_t *) malloc(source_len * sizeof(struct rx_stats_t));
		if(rx_stats == NULL)
			return ENOTSUP;

		memset(rx_stats, '\0', source_len * sizeof(struct rx_stats_t));
		
		return SUCCESS;
	}else if(p->uid == RIME_EXAMPLEUNICASTTXSTATS){

		// Set TxxStat values
		memcpy(&tx_stats, value, p->len);
//		printf("tx_stats SET source=%u latency_sum=%lu latency_count=%u\n", uc_from, tx_stats.latency_sum, tx_stats.latency_count);

		return SUCCESS;
	}else if(p->uid == RIME_EXAMPLEUNICASTRXSTATS){

		// sender ID is sent through type parameter
		memcpy(&uc_from, &(p->type), sizeof(uint8_t));

		// Search for sender ID
		int i;
		for(i = 0; i < source_len; i++)
		{
			if(linkaddr_cmp(&(source_IDs[i]), &uc_from))
			{
				// Set RxStat values
				memcpy(&(rx_stats[i]), value, p->len);
//				printf("SET source=%u rssi_sum=%d lqi_sum=%u packet_recvd=%u packet_lost=%u seq_nr=%u\n", uc_from, rx_stats[i].rssi_sum, rx_stats[i].lqi_sum, rx_stats[i].packet_recvd, rx_stats[i].packet_lost, rx_stats[i].seq_nr);

				return SUCCESS;
			}
		}

		return ENOTSUP;
	}
	PRINTF(" not found\n");
	return ENOTSUP;
}

param_t RIME_exampleUnicastSendInterval	= {{RIME_EXAMPLEUNICASTSENDINTERVAL},	getParameter, setParameter};
param_t RIME_exampleUnicastActivateApp	= {{RIME_EXAMPLEUNICASTACTIVATEAPP},	getParameter, activateAPP};
param_t RIME_exampleUnicastMsgSize	= {{RIME_EXAMPLEUNICASTMSGSIZE},	getParameter, setParameter};
param_t RIME_exampleUnicastReceiver	= {{RIME_EXAMPLEUNICASTRECEIVER},	getParameter, setParameter};
param_t RIME_exampleUnicastSourceIDs	= {{RIME_EXAMPLEUNICASTSOURCEIDS},	getParameter, setParameter};
param_t RIME_exampleUnicastTXStats	= {{RIME_EXAMPLEUNICASTTXSTATS},	getParameter, setParameter};
param_t RIME_exampleUnicastRXStats	= {{RIME_EXAMPLEUNICASTRXSTATS},	getParameter, setParameter};

PROCESS_THREAD(uart_process, ev, data)
{

	PROCESS_BEGIN();

	while(1){
		PROCESS_WAIT_EVENT();
		if (ev == PROCESS_EVENT_POLL ){
			int i;
			for(i = 0; i < source_len; i++)
			{
				if(linkaddr_cmp(&(source_IDs[i]), &uc_from))
				{
					uint16_t curr_seq_nr;
					memcpy(&curr_seq_nr,packetbuf_dataptr(),sizeof(uint16_t));

					rx_stats[i].rssi_sum += packetbuf_attr(PACKETBUF_ATTR_RSSI);
					rx_stats[i].lqi_sum  += packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);
					rx_stats[i].packet_recvd++;
					if (curr_seq_nr > rx_stats[i].seq_nr)
					{
						rx_stats[i].packet_lost += (curr_seq_nr - rx_stats[i].seq_nr - 1);
					}
					else
					{
						rx_stats[i].packet_lost += (65535 - curr_seq_nr + rx_stats[i].seq_nr);
					}
					rx_stats[i].seq_nr = curr_seq_nr;

//					printf("source=%u rssi_sum=%d lqi_sum=%u packet_recvd=%u packet_lost=%u seq_nr=%u\n", uc_from.u8[0], rx_stats[i].rssi_sum, rx_stats[i].lqi_sum, rx_stats[i].packet_recvd, rx_stats[i].packet_lost, rx_stats[i].seq_nr);

					break;
				}
			}
		}
	}
  
	PROCESS_END();
  
}

static void recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
	#if DEBUG
	uint32_t curr_timestamp = clock_usec();
	PRINTF("unicast message LEN %u received from %d.%d @ %lu\n",packetbuf_datalen(),from->u8[0], from->u8[1],curr_timestamp);
	#endif
	memcpy(&uc_from,from,2);
	process_poll(&uart_process);
}

static void sent_uc(struct unicast_conn *ptr, int status, int num_tx){
	uint32_t latency = clock_usec() - start_time_us;
//	if(status == MAC_TX_OK)
//	{
		tx_stats.latency_sum += latency;
		tx_stats.latency_count++;
//		printf("source=%u seq_nr=%u latency=%lu latency_sum=%lu latency_count=%u status=%u\n", linkaddr_node_addr.u8[0], msg.hdr.seq, latency, tx_stats.latency_sum, tx_stats.latency_count, status);
//	}
}
static const struct unicast_callbacks unicast_callbacks = {recv_uc,sent_uc};

/*---------------------------------------------------------------------------*/
static struct etimer et;

PROCESS_THREAD(example_unicast_process, ev, data)
{
	PROCESS_EXITHANDLER(unicast_close(&uc); etimer_stop(&et);)

	PROCESS_BEGIN();
	unicast_open(&uc, 146, &unicast_callbacks);
	if(data == NULL){
		process_start(&uart_process, NULL);
		param_repo_add_parameter(&RIME_exampleUnicastSendInterval);
		param_repo_add_parameter(&RIME_exampleUnicastActivateApp);
		param_repo_add_parameter(&RIME_exampleUnicastMsgSize);
		param_repo_add_parameter(&RIME_exampleUnicastReceiver);
		param_repo_add_parameter(&RIME_exampleUnicastSourceIDs);
		param_repo_add_parameter(&RIME_exampleUnicastTXStats);
		param_repo_add_parameter(&RIME_exampleUnicastRXStats);
	}
	if(applicationActive){
		random_init(clock_usec());
		while(1) {
			current_send_interval = random_rand() * send_interval / RANDOM_RAND_MAX;
			if(current_send_interval > 0){
				etimer_set(&et, current_send_interval);
				PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
			}
			if(!linkaddr_cmp(&receiver, &linkaddr_node_addr)) {
				start_time_us = clock_usec();
				msg.hdr.seq++;
				msg.hdr.ts = start_time_us;
				packetbuf_copyfrom(&msg, msg_size);
				unicast_send(&uc, &receiver);
			}
		}
	}
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/


