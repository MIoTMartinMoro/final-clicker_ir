#include <stdio.h>
#include <xc.h>

#include <contiki.h>
#include <contiki-net.h>

#include "dev/leds.h"
#include "lib/memb.h"
#include "sys/clock.h"

#include "letmecreate/click/led_matrix.h"
#include "letmecreate/core/common.h"
#include "letmecreate/core/debug.h"
#include "letmecreate/core/network.h"
#include "letmecreate/core/spi.h"
#include "mqtt.h"

#include "common.h"
#include "fsm.h"

#include <lib/sensors.h>
#include <pic32_gpio.h>

#define N_BUTTONS   2
#define N_COL_LEDS  8

#define FLAG_BUTTON_1 1
#define FLAG_BUTTON_2 2

static uint8_t buttons_flags_status = 0; //0 apagado, 1 pulsado [0] -> boton1, [1]->boton2
static uint8_t dibujos[7][N_COL_LEDS] = { 
    { 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00 },  // Config 1
    { 0x00, 0x00, 0x3C, 0x24, 0x24, 0x3C, 0x00, 0x00 },  // Config 2
    { 0x00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00 },  // Config 3
    { 0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF },  // Config 4
    { 0xFF, 0xA1, 0x91, 0x89, 0x85, 0x89, 0x91, 0xFF },  // Empty
    { 0x00, 0xC3, 0xA5, 0x99, 0x99, 0xA5, 0xC3, 0x00 },  // Waiter called
    { 0x00, 0x22, 0x41, 0x55, 0x55, 0x55, 0x3E, 0x14 }   // Bill asked
};

MEMB(appdata, struct idappdata, MAXDATASIZE);

/*---------------------------------------------------------------------------*/
void init_buttons()
{
    //inicialización botón izquierda
    ANSELEbits.ANSE7 = 0;
    TRISEbits.TRISE7 = 1;
    LATEbits.LATE7 = 0;

    //inicialización botón derecha
    ANSELBbits.ANSB0 = 0;
    TRISBbits.TRISB0 = 1;
    LATBbits.LATB0 = 0;    
}
/*---------------------------------------------------------------------------*/
void button_isr(int button_id, int flag)
{
    static struct timer debouncetimer[N_BUTTONS];
    static uint8_t button_pressed[N_BUTTONS];

    if (!button_pressed[button_id]) {
        /*
        * If timer is still running, it means the button has just been released and
        * it is a false notification due to bouncing
        */
        if (timer_expired(&debouncetimer[button_id])) {
        button_pressed[button_id] = 1;
            /* Set a timer for 100ms to ignore false notifications due to bouncing */
            timer_set(&debouncetimer[button_id], CLOCK_SECOND / 10);
            /* Pone a 1 el flag del botón pulsado */
            buttons_flags_status |= flag;
        }
    } else {
        /*
        * If timer is still running, it means the button has just been pressed and
        * it is a false notification due to bouncing
        */
        if (timer_expired(&debouncetimer[button_id])) {
            button_pressed[button_id] = 0;
            /* Set a timer for 100ms to ignore false notifications due to bouncing */
            timer_set(&debouncetimer[button_id], CLOCK_SECOND / 10);
        }
    }
}
/*---------------------------------------------------------------------------*/
void button_callback(void)
{
    if (PORTBbits.RB0 == 0) {  // Si se pulsa boton derecha
        button_isr(1, FLAG_BUTTON_2);  // Pone a 1 su flag
    }
    if (PORTEbits.RE7 == 0) {  // Si se pulsa boton izquierda
        button_isr(0, FLAG_BUTTON_1);  // Pone a 1 su flag
    }
}
/*---------------------------------------------------------------------------*/
void send_alert_occupated(fsm_t* fsm)
{
    static char topic[50];
    static struct idappdata* envio;
    static uint16_t mid = 0;
    envio = memb_alloc(&appdata);

    leds_on(LED1);

    memset(envio->data, '\0', MAXDATASIZE - ID_HEADER_LEN);

    sprintf(envio->data, "Mesa %d ocupada", fsm->id_clicker & 0x3F);
    envio->op = OP_MESA_OCUPADA;
    envio->id = (fsm->id_clicker << 8) + fsm->id_msg;  // El primer byte se corresponde con el id de la mesa y el segundo con el nº de mensaje enviado
    mid = envio->id;
    envio->len = strlen(envio->data);
    
    sprintf(topic, "restaurante/mesa/%d/ocupada", fsm->id_clicker & 0x3F);
    mqtt_publish(fsm->mqtt_conn, &mid, topic, (char*) envio, ID_HEADER_LEN + envio->len, 1, 0);
    udp_packet_send(fsm->conn, (char*) envio, ID_HEADER_LEN + envio->len);
    fsm->id_msg++;

    led_matrix_click_display_number(fsm->id_clicker & 0x3F);
    
    memb_free(&appdata, envio);
    leds_off(LED1);
}
/*---------------------------------------------------------------------------*/
void send_alert_attended(fsm_t* fsm)
{
    static char topic[50];
    static struct idappdata* envio;
    static uint16_t mid = 0;
    envio = memb_alloc(&appdata);

    leds_on(LED1);

    memset(envio->data, '\0', MAXDATASIZE - ID_HEADER_LEN);

    sprintf(envio->data, "Mesa %d atendida", fsm->id_clicker & 0x3F);
    envio->op = OP_MESA_ATENDIDA;
    envio->id = (fsm->id_clicker << 8) + fsm->id_msg;  // El primer byte se corresponde con el id de la mesa y el segundo con el nº de mensaje enviado
    mid = envio->id;
    envio->len = strlen(envio->data);
    
    sprintf(topic, "restaurante/mesa/%d/atendida", fsm->id_clicker & 0x3F);
    mqtt_publish(fsm->mqtt_conn, &mid, topic, (char*) envio, ID_HEADER_LEN + envio->len, 1, 0);
    udp_packet_send(fsm->conn, (char*) envio, ID_HEADER_LEN + envio->len);
    fsm->id_msg++;

    led_matrix_click_display_number(fsm->id_clicker & 0x3F);
    
    memb_free(&appdata, envio);
    leds_off(LED1);
}
/*---------------------------------------------------------------------------*/
void send_alert_empty(fsm_t* fsm)
{
    static char topic[50];
    static struct idappdata* envio;
    static uint16_t mid = 0;
    envio = memb_alloc(&appdata);

    leds_on(LED1);

    memset(envio->data, '\0', MAXDATASIZE - ID_HEADER_LEN);

    sprintf(envio->data, "Mesa %d vaciada", fsm->id_clicker & 0x3F);
    envio->op = OP_MESA_VACIA;
    envio->id = (fsm->id_clicker << 8) + fsm->id_msg;  // El primer byte se corresponde con el id de la mesa y el segundo con el nº de mensaje enviado
    mid = envio->id;
    envio->len = strlen(envio->data);
    
    sprintf(topic, "restaurante/mesa/%d/vaciada", fsm->id_clicker & 0x3F);
    mqtt_publish(fsm->mqtt_conn, &mid, topic, (char*) envio, ID_HEADER_LEN + envio->len, 1, 0);
    udp_packet_send(fsm->conn, (char*) envio, ID_HEADER_LEN + envio->len);
    fsm->id_msg++;

    led_matrix_click_set(dibujos[4]);
    
    memb_free(&appdata, envio);
    leds_off(LED1);
}
/*---------------------------------------------------------------------------*/
void send_alert_bill(fsm_t* fsm)
{
    static char topic[50];
    static struct idappdata* envio;
    static uint16_t mid = 0;
    envio = memb_alloc(&appdata);

    leds_on(LED1);

    memset(envio->data, '\0', MAXDATASIZE - ID_HEADER_LEN);

    sprintf(envio->data, "Mesa %d cuenta pedida", fsm->id_clicker & 0x3F);
    envio->op = OP_MESA_CUENTA;
    envio->id = (fsm->id_clicker << 8) + fsm->id_msg;  // El primer byte se corresponde con el id de la mesa y el segundo con el nº de mensaje enviado
    mid = envio->id;
    envio->len = strlen(envio->data);
    
    sprintf(topic, "restaurante/mesa/%d/cuenta", fsm->id_clicker & 0x3F);
    mqtt_publish(fsm->mqtt_conn, &mid, topic, (char*) envio, ID_HEADER_LEN + envio->len, 1, 0);
    udp_packet_send(fsm->conn, (char*) envio, ID_HEADER_LEN + envio->len);
    fsm->id_msg++; 

    led_matrix_click_set(dibujos[6]);
    
    memb_free(&appdata, envio);
    leds_off(LED1);
}
/*---------------------------------------------------------------------------*/
void send_alert_waiter_call(fsm_t* fsm)
{
    static char topic[50];
    static struct idappdata* envio;
    static uint16_t mid = 0;
    envio = memb_alloc(&appdata);

    leds_on(LED1);

    memset(envio->data, '\0', MAXDATASIZE - ID_HEADER_LEN);

    sprintf(envio->data, "Mesa %d llama camarero", fsm->id_clicker & 0x3F);
    envio->op = OP_MESA_LLAMA;
    envio->id = (fsm->id_clicker << 8) + fsm->id_msg;  // El primer byte se corresponde con el id de la mesa y el segundo con el nº de mensaje enviado
    mid = envio->id;
    envio->len = strlen(envio->data);

    sprintf(topic, "restaurante/mesa/%d/llamada", fsm->id_clicker & 0x3F);
    mqtt_publish(fsm->mqtt_conn, &mid, topic, (char*) envio, ID_HEADER_LEN + envio->len, 1, 0);
    udp_packet_send(fsm->conn, (char*) envio, ID_HEADER_LEN + envio->len);
    fsm->id_msg++; 
    
    memb_free(&appdata, envio);
    led_matrix_click_set(dibujos[5]);
    leds_off(LED1);
}
/*---------------------------------------------------------------------------*/
/* 
 * Funciones de lectura de sensores para comprobar el cambio de estado.
 * Devuelven 1 si queremos cambiar, 0 si no
 *
 */
int check_button1(fsm_t* fsm){
    if ((buttons_flags_status & 1) == 1) {
        buttons_flags_status = 0;
        return 1;
    } else {
        return 0;
    }
}
/*---------------------------------------------------------------------------*/
int check_button2(fsm_t* fsm) {
    if ((buttons_flags_status & 2) == 2) {
        buttons_flags_status = 0;
        return 1;
    } else {
        return 0;
    }
}
/*---------------------------------------------------------------------------*/
/* Función necesaria para usar MQTT */
void mqtt_event(struct mqtt_connection* m, mqtt_event_t event, void* data)
{
    
}
/*---------------------------------------------------------------------------*/
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
        static struct mqtt_connection mqtt_conn;
        static struct uip_ip_hdr metadata;
        static struct uip_udp_conn* conn;
        static uint8_t flashes = 0;
        static uint8_t i = 0;
        static uint8_t id_clicker = 0;
        static uint8_t id_msg = 0;
        static uint8_t num_mesa = 0;

        enum fsm_state { EMPTY, OCCUPATED, BILL_ASKED, WAITER_CALLED };
        
        static fsm_trans_t mesa_tt[] = {
            {EMPTY,          check_button1,  OCCUPATED,      send_alert_occupated},
            {OCCUPATED,      check_button1,  WAITER_CALLED,  send_alert_waiter_call},
            {OCCUPATED,      check_button2,  BILL_ASKED,     send_alert_bill},
            {WAITER_CALLED,  check_button1,  OCCUPATED,      send_alert_attended},
            {WAITER_CALLED,  check_button2,  BILL_ASKED,     send_alert_bill},
            {BILL_ASKED,     check_button2,  EMPTY,          send_alert_empty},
            {-1, NULL, -1, NULL},
        }; 

        operacion = memb_alloc(&appdata);
        resultado = memb_alloc(&appdata);

        PRINTF("=====Start=====\n");

        // CONFIGURACIÓN SPI
        if(spi_init() < 0 ||
            spi_set_mode(MIKROBUS_1, SPI_MODE_3) < 0)
        {
            PRINTF("SPI init failed\n");
            PROCESS_EXIT();
        }

        PRINTF("SPI init passed\n");    

        // CONFIGURACIÓN LED_MATRIX
        if(led_matrix_click_enable() < 0 ||
           led_matrix_click_set_intensity(1) < 0)
        {
            PRINTF("LED matrix init failed\n");
            PROCESS_EXIT();
        }

        led_matrix_click_set(dibujos[0]);

        flashes = 2;
        while(flashes--) {
              /* Flash every second */
            for(i = 0; i < 20; i++)
                clock_delay_usec(50000);

            leds_toggle(LED1);
        }

        // CONFIGURACIÓN SOCKET UDP
        conn = udp_new_connection(PUERTO_CLIENTE, PUERTO_SERVIDOR, IP6_CI40);
        PROCESS_WAIT_UDP_CONNECTED();
        PROCESS_WAIT_UDP_CONNECTED();
        PROCESS_WAIT_UDP_CONNECTED();
        ipv6_add_default_route(IP6_CI40, NETWORK_INFINITE_LIFETIME);

        led_matrix_click_set(dibujos[1]);
        
        init_buttons();

        flashes = 4;
        while(flashes--) {
            /* Flash every second */
            for(i = 0; i < 20; i++)
                clock_delay_usec(50000);

            leds_toggle(LED1);
        }

        PRINTF("Quien soy?\n");

        // ENVIAR MENSAJE AL SERVIDOR PIDIENDO UN ID
        memset(operacion->data, '\0', MAXDATASIZE - ID_HEADER_LEN);
        strcpy(operacion->data, "Quien soy (MESA)");
        operacion->op = OP_WHOAMI_MESA;
        operacion->id = (uint16_t) ((MESA_PREF + id_clicker) << 8) + id_msg;  // El primer byte se corresponde con el id de la mesa y el segundo con el nº de mensaje enviado
        operacion->len = strlen(operacion->data);

        PRINTF("(Enviado) OP: 0x%X ID: %ld Len: %ld Data: %s\n", operacion->op, operacion->id, operacion->len, operacion->data);

        leds_on(LED1);
        leds_on(LED2);
        led_matrix_click_set(dibujos[2]);
        udp_packet_send(conn, (char*) operacion, ID_HEADER_LEN + operacion->len);
        id_msg++;
        PROCESS_WAIT_UDP_SENT();
        leds_off(LED1);
        PROCESS_WAIT_UDP_RECEIVED();
        leds_off(LED2);
        memset(buffer, '\0', MAXDATASIZE + 1);
        udp_packet_receive(buffer, MAXDATASIZE, &metadata);
        led_matrix_click_set(dibujos[3]);

        resultado = (struct idappdata*) &buffer;

        num_mesa= (uint8_t) strtol(resultado->data, NULL, 10);
        id_clicker = (uint8_t) (MESA_PREF + num_mesa);

        PRINTF("(Recibido) OP: 0x%X ID: %ld Len: %ld Data: %s\n", resultado->op, resultado->id, resultado->len, resultado->data);

        // CONEXIÓN CON EL BROKER MQTT
        if (mqtt_register(&mqtt_conn, &main_process, &id_clicker, mqtt_event, MAXDATASIZE) < 0) {
            PRINTF("MQTT register failed\n");
            PROCESS_EXIT();
        }

        if (mqtt_connect(&mqtt_conn, IP6_CI40, PUERTO_MQTT, 99) < 0) {
            PRINTF("MQTT connect failed\n");
            PROCESS_EXIT();
        }

        // CREACIÓN DE LA MÁQUINA DE ESTADOS
        fsm = fsm_new(mesa_tt, id_clicker, id_msg, conn, &mqtt_conn);

        PRINTF("********FSM CREADA********\n");

        PRINTF("Starting loop\n");

        led_matrix_click_set(dibujos[4]);

        // COMIENZO DEL BUCLE
        while(1)
        {
            etimer_set(&et, CLOCK_SECOND/2);

            button_callback();
            fsm_fire(fsm);

            PROCESS_WAIT_EVENT();
        }
        
        led_matrix_click_disable();
        spi_release();
        memb_free(&appdata, operacion);
        memb_free(&appdata, resultado);
    }
    PROCESS_END();
}

/*---------------------------------------------------------------------------*/
