#include <stdio.h>
#include <xc.h>

#include <contiki.h>
#include <contiki-net.h>

#include "sys/clock.h"
#include "dev/leds.h"

#include "letmecreate/core/network.h"
#include "letmecreate/core/debug.h"
#include "letmecreate/core/common.h"
#include "letmecreate/core/spi.h"
#include "lib/memb.h"

#include "common.h"
#include "fsm.h"
#include "mcp3004.h"

#define UMBRAL 2
#define N_PLATOS 4

MEMB(appdata, struct idappdata, MAXDATASIZE);

uint8_t convert_values(float* values)
{
    uint8_t n = sizeof(values) / sizeof(float);
    uint8_t resp = 0;
    uint8_t i;

    for (i = 0; i < n; i++) {
        if (values[i] > UMBRAL) {
            resp |= (1 << i);
        }
    }

    return resp;
}

//Funciones de salida. Hacen una acción. Devuelven void
void send_alert(fsm_t* fsm)
{
    uint8_t i;
    static struct idappdata* envio;
    envio = memb_alloc(&appdata);

    leds_on(LED1);

    memset(envio->data, '\0', MAXDATASIZE - ID_HEADER_LEN);

    for (i = 0; i < N_PLATOS; i++) {
        if ((fsm->ir_new_state & (1 << i)) && ((fsm->ir_new_state ^ fsm->ir_state) & (1 << i))) {
            sprintf(envio->data, "Plato %ld detectado", i);
            envio->op = OP_PLATO_DETECTADO;
            envio->id = fsm->id_msg + (i << 8);
            envio->len = strlen(envio->data);

            udp_packet_send(fsm->conn, (char*) envio, ID_HEADER_LEN + envio->len);
            int flashes = 6;
            while(flashes--) {
                  /* Flash every second */
                for(i = 0; i < 20; i++)
                    clock_delay_usec(50000);
            }
        }
    }
    memb_free(&appdata, envio);
    fsm->ir_state = fsm->ir_new_state;
    leds_off(LED1);
}

void clear_alert(fsm_t* fsm){
    printf("Nada para llevar \n");
    //envío paquete UDP
    leds_off(LED1);
}

//Funciones de lectura de sensores para comprobar el cambio de estado. Devuelven 1 si queremos cambiar, 0 si no

int check_sensor(fsm_t* fsm)
{
    static float values[N_PLATOS];
    static uint8_t ir_values;
    mcp3004_read_all_channels(values);
    ir_values = convert_values(values);
    fsm->ir_new_state = ir_values;

    return ir_values;
}

int check_sensor_none(fsm_t* fsm){
    printf("comprueba valor sensor IR \n");
    static float value = 0.0f;
    static uint8_t channel =1;
    mcp3004_read_channel(channel, &value);
    printf("VALUE: %f\n", value);
    if(value < 2){ //boton izquierda
        printf("no hay plato \n");
        return 1;
    }else{
        return 0;
    }
}


PROCESS(main_process, "Main process");
AUTOSTART_PROCESSES(&main_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(main_process, ev, data)
{
    PROCESS_BEGIN();
    INIT_NETWORK_DEBUG();
    {
        static char buffer[MAXDATASIZE + 1];
        static fsm_t* fsm;
        static struct etimer et;
        static struct idappdata* operacion;
        static struct idappdata* resultado;
        static struct uip_ip_hdr metadata;
        static struct uip_udp_conn* conn;
        static uint8_t i;
        static uint8_t id_clicker = 0;
        static uint8_t id_msg = 0;
        static uint16_t minor, major;

        enum fsm_state { EMPTY, DETECTED };
        
        static fsm_trans_t sensor_ir[] = {
            {EMPTY,     check_sensor,        DETECTED,  send_alert},
            {DETECTED,  check_sensor_none,   EMPTY,     clear_alert},
            {DETECTED,  check_sensor_none,   DETECTED,  clear_alert},
            {-1, NULL, -1, NULL},
        }; 

        operacion = memb_alloc(&appdata);
        resultado = memb_alloc(&appdata);

        PRINTF("=====Start=====\n");

        if(spi_init() < 0 ||
            spi_set_mode(MIKROBUS_1, SPI_MODE_3) < 0)
        {
            PRINTF("SPI init failed\n");
            return 1;
        }

        PRINTF("SPI init passed\n");

        int flashes = 2;
        while(flashes--) {
              /* Flash every second */
            for(i = 0; i < 20; i++)
                clock_delay_usec(50000);

            leds_toggle(LED1);
        }

        conn = udp_new_connection(PUERTO_CLIENTE, PUERTO_SERVIDOR, IP6_CI40);
        PROCESS_WAIT_UDP_CONNECTED();
        PROCESS_WAIT_UDP_CONNECTED();
        PROCESS_WAIT_UDP_CONNECTED();
        ipv6_add_default_route(IP6_CI40, NETWORK_INFINITE_LIFETIME);
        
        flashes = 4;
        while(flashes--) {
            /* Flash every second */
            for(i = 0; i < 20; i++)
                clock_delay_usec(50000);

            leds_toggle(LED1);
        }

        PRINTF("Quien soy?\n");

        memset(operacion->data, '\0', MAXDATASIZE - ID_HEADER_LEN);
        strcpy(operacion->data, "Quien soy (IR)");
        operacion->op = OP_WHOAMI_IR;
        operacion->id = (uint16_t) ((IR_PREF + id_clicker) << 8) + id_msg;
        operacion->len = strlen(operacion->data);

        PRINTF("(Enviado) OP: 0x%X ID: %ld Len: %ld Data: %s\n", operacion->op, operacion->id, operacion->len, operacion->data);

        leds_on(LED1);
        leds_on(LED2);
        udp_packet_send(conn, (char*) operacion, ID_HEADER_LEN + operacion->len);
        id_msg++;
        PROCESS_WAIT_UDP_SENT();
        leds_off(LED1);
        PROCESS_WAIT_UDP_RECEIVED();
        leds_off(LED2);
        memset(buffer, '\0', MAXDATASIZE + 1);
        udp_packet_receive(buffer, MAXDATASIZE, &metadata);

        resultado = (struct idappdata*) &buffer;

        id_clicker = (uint8_t) (IR_PREF + (uint8_t) strtol(resultado->data, NULL, 10));

        PRINTF("(Recibido) OP: 0x%X ID: %ld Len: %ld Data: %s\n", resultado->op, resultado->id, resultado->len, resultado->data);

        fsm = fsm_new(sensor_ir, (id_clicker << 8) + id_msg, conn);

        PRINTF("********FSM CREADA********\n");

        PRINTF("Starting loop\n");

        while(1)
        {
            etimer_set(&et, CLOCK_SECOND);

            fsm_fire(fsm);

            PROCESS_WAIT_EVENT();
        }

        spi_release();
        memb_free(&appdata, operacion);
        memb_free(&appdata, resultado);
    }
    PROCESS_END();
}

/*---------------------------------------------------------------------------*/
