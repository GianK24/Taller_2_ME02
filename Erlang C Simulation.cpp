
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lcgrand.cpp"  /* Encabezado para el generador de numeros aleatorios */
#include <iostream>

#define LIMITE_COLA 100  /* Capacidad maxima de la cola */
#define OCUPADO      1  /* Indicador de Servidor Ocupado */
#define LIBRE      0  /* Indicador de Servidor Libre */

using namespace std;

int   sig_tipo_evento, num_clientes_espera, num_esperas_requerido, num_eventos, servidores_ocupados,
      num_entra_cola, *estado_servidores, CodError, num_servidores, servidor_proximo, stream;
float area_num_entra_cola, *area_estado_servidores, media_entre_llegadas, media_atencion,
      tiempo_simulacion, tiempo_llegada[LIMITE_COLA + 1], tiempo_ultimo_evento, tiempo_sig_evento[3],
      total_de_esperas, *tiempos_salida, erlangC;
FILE  *parametros, *resultados;

void  SimuladorPrincipal(void);
void  inicializar(void);
void  controltiempo(void);
void  llegada(void);
void  salida(void);
void  reportes(void);
void  actualizar_estad_prom_tiempo(void);
float expon(float media);

int buscarServidorLibre(); /* Función auxiliar para proceso de varios servidores */

int main(void) {  /* Funcion Principal */

    /* Abre los archivos de entrada y salida */

    parametros  = fopen("param.txt",  "r");
    resultados = fopen("result.txt", "w");

    /* Especifica el numero de eventos para la funcion controltiempo. */
    num_eventos = 2;

    /* Lee los parametros de entrada. */
    fscanf(parametros, "%f %f %d %d %d", &media_entre_llegadas, &media_atencion,
           &num_esperas_requerido, &num_servidores, &stream);

    SimuladorPrincipal();

    fclose(parametros);
    fclose(resultados);

    return 0;
}

void SimuladorPrincipal(void){ /* Función SimuladorPrincipal */

    /* Escribe en el archivo de salida los encabezados del reporte y los parametros iniciales */
    fprintf(resultados, "Sistema de Colas M/M/m\n\n");
    fprintf(resultados, "Tiempo promedio de llegada%11.3f minutos\n\n",
            media_entre_llegadas);
    fprintf(resultados, "Tiempo promedio de atencion%16.3f minutos\n\n", media_atencion);
    fprintf(resultados, "Numero de clientes%14d\n\n", num_esperas_requerido);
    fprintf(resultados, "Numero de servidores%14d\n\n", num_servidores);

    /* Inicializa la simulacion. */
    inicializar();  // PROCEDIMIENTO INICIALIZACIÓN

    CodError = 0;

    /* Corre la simulacion mientras no se llegue al numero de clientes especificaco en el archivo de entrada*/
    while ((num_clientes_espera < num_esperas_requerido) && (CodError == 0)) {
        /* Determina el siguiente evento */
        controltiempo(); // ManejoTiempoEspacio
        /* Invoca la funcion del evento adecuado. */
        switch (sig_tipo_evento) {
            case 1:
                llegada();          // FUNCIÓN EVENTO_1
                break;
            case 2:
                salida();           // FUNCIÓN EVENTO_2
                break;
            default:
                CodError = 1;       // Error no manejado
                break;
        }
    }
    
    /* Invoca el generador de reportes y termina la simulacion. */
    reportes(); // PROCEDIMIENTO GeneradorReporte
    delete[] estado_servidores, tiempos_salida, area_estado_servidores;
}

void inicializar(void)  /* Funcion de Inicialización. */
{
    /* Inicializa el reloj de la simulacion. */
    tiempo_simulacion       = 0.0;

    /* Inicializa las variables de estado */
    estado_servidores       = new int[num_servidores];
    tiempos_salida          = new float[num_servidores];
    
    for(int i=0; i<num_servidores;i++){
      estado_servidores[i] = LIBRE;
      tiempos_salida[i] = 1.0e+30;
    }
    servidor_proximo = -1;
    servidores_ocupados     = 0;
    num_entra_cola          = 0;
    tiempo_ultimo_evento    = 0.0;
    num_clientes_espera     = 0;

    /* Inicializa los contadores estadisticos. */
    total_de_esperas        = 0.0;
    area_num_entra_cola     = 0.0;
    area_estado_servidores  = new float[num_servidores];
    for(int i=0; i<num_servidores;i++){
      area_estado_servidores[i] = 0;
    }
    erlangC = 0;

    /* Inicializa la lista de eventos. Ya que no hay clientes, el evento salida
       (terminacion del servicio) no se tiene en cuenta */
    tiempo_sig_evento[1]    = tiempo_simulacion + expon(media_entre_llegadas);
    tiempo_sig_evento[2]    = 1.0e+30;
}

void controltiempo(void)  /* Funcion ManejoTiempoEspacio */
{
    int   i;
    float min_tiempo_sig_evento = 1.0e+29;
    sig_tipo_evento = 0;

    /* Se determina servidor más proximo a ser liberado */
    float min_server = 1.0e+29;
    for(int j = 0; j < num_servidores; j++){
      if (estado_servidores[j]==OCUPADO && tiempos_salida[j]<min_server){
        min_server = tiempos_salida[j];
        servidor_proximo = j;
      }
    }
    tiempo_sig_evento[2] = min_server;

    /*  Determina el tipo de evento del evento que debe ocurrir. */
    for (i = 1; i <= num_eventos; ++i)                          //Próximo evento
        if (tiempo_sig_evento[i] < min_tiempo_sig_evento) {
            min_tiempo_sig_evento = tiempo_sig_evento[i];
            sig_tipo_evento     = i;
        }

    /* Revisa si la lista de eventos esta vacia. */
    if (sig_tipo_evento == 0) {
        /* La lista de eventos esta vacia, se detiene la simulacion. */
        fprintf(resultados, "\nLa lista de eventos esta vacia %f", tiempo_simulacion);
        CodError = 1;
        return;
    }

    /* La lista de eventos no esta vacia, adelanta el reloj de la simulacion. */
    tiempo_simulacion = min_tiempo_sig_evento;  //Actualizar el Tiempo Espacio

    /* Actualiza los acumuladores estadisticos de tiempo promedio */
    actualizar_estad_prom_tiempo();
}

void llegada(void)  /* Funcion evento de llegada (EVENTO_1) */
{
    float espera;

    /* Programa la siguiente llegada. */
    tiempo_sig_evento[1] = tiempo_simulacion + expon(media_entre_llegadas);

    /* Revisa si todos los servidores estan en estado OCUPADO. */
    if (servidores_ocupados == num_servidores) {
        /* Servidor OCUPADO, aumenta el numero de clientes en cola */
        ++num_entra_cola;

        /* Verifica si hay condición de desbordamiento */
        if (num_entra_cola > LIMITE_COLA) {
            /* Se ha desbordado la cola, detiene la simulacion */
            fprintf(resultados, "\nDesbordamiento del arreglo tiempo_llegada a la hora");
            fprintf(resultados, "%f", tiempo_simulacion);
            CodError = 2;
            return;
        }

        /* Todavia hay espacio en la cola, se almacena el tiempo de llegada del
        	cliente en el ( nuevo ) fin de tiempo_llegada */
        tiempo_llegada[num_entra_cola] = tiempo_simulacion;
    }
    else {
        /*  El servidor esta LIBRE, por lo tanto el cliente que llega tiene tiempo de eespera=0
           (Las siguientes dos lineas del programa son para claridad, y no afectan
           el reultado de la simulacion ) */
        espera            = 0.0;
        total_de_esperas += espera;

        /* Incrementa el numero de clientes en espera, y pasa el servidor libre a ocupado */
        ++num_clientes_espera;
        int servidor = buscarServidorLibre();
        estado_servidores[servidor] = OCUPADO;
        tiempos_salida[servidor] = tiempo_simulacion + expon(media_atencion);
        servidores_ocupados++;
    }
}

int buscarServidorLibre(){
    if(servidores_ocupados == num_servidores) return -1;
    for(int i=0; i < num_servidores ; i++){
      if (estado_servidores[i] == LIBRE){
        return i;
      }
    }
    return -1;
}

void salida(void)  /* Funcion evento de salida (EVENTO_2) */
{
    int   i;
    float espera;
    /* Revisa si la cola esta vacia */
    if (num_entra_cola == 0) {
        /* La cola esta vacia, pasa los servidores a LIBRE y
        no considera el evento de salida*/
        servidores_ocupados--;
        tiempos_salida[servidor_proximo] = 1.0e+30;
        estado_servidores[servidor_proximo] = LIBRE;
    }
    else {
        /* La cola no esta vacia, disminuye el numero de clientes en cola. */
        --num_entra_cola;

        /*Calcula la espera del cliente que esta siendo atendido y
        actualiza el acumulador de espera */
        espera            = tiempo_simulacion - tiempo_llegada[1];
        total_de_esperas += espera;

        /*Incrementa el numero de clientes en espera, y programa la salida. */
        ++num_clientes_espera;
        tiempos_salida[servidor_proximo] = tiempo_simulacion + expon(media_atencion);

        /* Mueve cada cliente en la cola ( si los hay ) una posicion hacia adelante */
        for (i = 1; i <= num_entra_cola; ++i)
            tiempo_llegada[i] = tiempo_llegada[i + 1];
    }
}

void reportes(void)  /* Funcion GeneradoraReporte. */
{
    /* Calcula y estima los estimados de las medidas deseadas de desempe�o */
    fprintf(resultados, "\n\nEspera promedio en la cola%11.3f minutos\n\n",
            total_de_esperas / num_clientes_espera);
    fprintf(resultados, "Numero promedio en cola%10.3f\n\n",
            area_num_entra_cola / tiempo_simulacion);
    fprintf(resultados, "Uso de Servidores\n\n");
    for(int i=0; i<num_servidores; i++){
        fprintf(resultados, "Uso del servidor %d : %15.3f\n", i+1,
            area_estado_servidores[i]/tiempo_simulacion);
    }
    fprintf(resultados, "\nErlang C de la simulacion%12.3f\n", erlangC/tiempo_simulacion);
    fprintf(resultados, "\nTiempo de terminacion de la simulacion%12.3f minutos", tiempo_simulacion);
}

void actualizar_estad_prom_tiempo(void)  /* Actualiza los acumuladores de
														area para las estadisticas de tiempo promedio. */
{
    float time_since_last_event;

    /* Calcula el tiempo desde el ultimo evento, y actualiza el marcador
    	del ultimo evento */
      
    time_since_last_event = tiempo_simulacion - tiempo_ultimo_evento;
    tiempo_ultimo_evento       = tiempo_simulacion;
    /* Actualiza el area bajo la funcion de numero_en_cola */
    area_num_entra_cola      += num_entra_cola * time_since_last_event;
    
    /*Actualiza el area bajo la funcion indicadora de servidor ocupado*/
    for(int i=0; i< num_servidores; i++){
      area_estado_servidores[i] += estado_servidores[i] * time_since_last_event;
    }

    if(servidores_ocupados == num_servidores) erlangC += time_since_last_event;
}

float expon(float media)  /* Funcion generadora de la exponencias */
{
    /* Retorna una variable aleatoria exponencial con media "media"*/
    return -media * log(lcgrand(stream));
}
