#ifndef FSM_H
#define FSM_H

#include <contiki-net.h>

typedef struct fsm_t fsm_t;

typedef int (*fsm_input_func_t) (fsm_t*);
typedef void (*fsm_output_func_t) (fsm_t*);

typedef struct fsm_trans_t {
	int orig_state;
	fsm_input_func_t in;
	int dest_state;
	fsm_output_func_t out;
} fsm_trans_t;

struct fsm_t {
	int current_state;
	int id_mesa;
	int id_msg;
	struct uip_udp_conn* conn;
	fsm_trans_t* tt;
};

fsm_t* fsm_new (fsm_trans_t* tt, int id, int id_msg, struct uip_udp_conn* conn);
void fsm_init (fsm_t* this, fsm_trans_t* tt, int id, int id_msg, struct uip_udp_conn* conn);
void fsm_fire (fsm_t* this);
	
#endif