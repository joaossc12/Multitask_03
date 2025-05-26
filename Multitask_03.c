#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "lib/ssd1306.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "pico/bootrom.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C
#define BOTAO_A 5 // Simula entrada
#define BOTAO_B 6 // Simula saida
#define JOYSTICK_BT 22 //Reset
#define LED_RED  13
#define LED_BLUE  12
#define LED_GREEN 11
#define BUZZER    21

#define CAPACIDADE 10

ssd1306_t ssd;

SemaphoreHandle_t xContadorSemph; //Será responsável pelo numero de VAGAS OCUPADAS
SemaphoreHandle_t xMutexDisplay; //Controla o acesso aos periféricos
SemaphoreHandle_t xBinSemphReset; //Acorda a task reset
SemaphoreHandle_t xBinSemphEntrada; //Acorda a task entrada
SemaphoreHandle_t xBinSemphSaida; //Acorda a task saida



// ISR para disparo dos semaforos binarios
void gpio_irq_handler(uint gpio, uint32_t events);
// Função ded atulização de Display e LED
void Atualiza(int numero);
//Função que toca o buzzer
void PlayBuzzer(uint16_t warp);
//Tarefa que registra e trata a entrada de veículos 
void vTaskEntrada(void *params);
//Tarefa que registra e trata a saída de veículos 
void vTaskSaida(void *params);
//Tarefa que RESETA o sistema
void vTaskReset(void *params);

int main()
{
    stdio_init_all();

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // Configura os botões
    gpio_init(JOYSTICK_BT);
    gpio_set_dir(JOYSTICK_BT, GPIO_IN);
    gpio_pull_up(JOYSTICK_BT);
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    gpio_set_irq_enabled_with_callback(JOYSTICK_BT, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BOTAO_A, GPIO_IRQ_EDGE_FALL, true);

    // Configura LED
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);

    //Configura Buzzer PWM
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER); 
    const uint16_t wrap = 1000;   // Valor de wrap do PWM
    pwm_set_enabled(slice, true);
    pwm_set_clkdiv(slice, 128);

    Atualiza(0); //Atualiza display


    // Cria semáforo de contagem (máximo CAPACIDADE, inicial 0)
    xContadorSemph = xSemaphoreCreateCounting(CAPACIDADE, 0);
    xMutexDisplay = xSemaphoreCreateMutex();
    xBinSemphReset = xSemaphoreCreateBinary();
    xBinSemphEntrada = xSemaphoreCreateBinary();
    xBinSemphSaida = xSemaphoreCreateBinary();

    xTaskCreate(vTaskEntrada, "Task Entrada", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskSaida, "Task Saida", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskReset, "Task Reset", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();

}
// ISR para disparando semaforos binarios
void gpio_irq_handler(uint gpio, uint32_t events)
{
    static absolute_time_t last_time_JB = 0;
    static absolute_time_t last_time_A = 0;
    static absolute_time_t last_time_B = 0;
    absolute_time_t now = get_absolute_time();
    if (gpio == JOYSTICK_BT){
        if (absolute_time_diff_us(last_time_JB, now) > 200000) { //Debouncing
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xBinSemphReset, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            last_time_JB = now;
        }
    }else if (gpio == BOTAO_A){
        if (absolute_time_diff_us(last_time_A, now) > 200000) { //Debouncing
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xBinSemphEntrada, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            last_time_A = now;
        }

    }else if (gpio == BOTAO_B){
        if (absolute_time_diff_us(last_time_B, now) > 200000) { //Debouncing
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xBinSemphSaida, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            last_time_B = now;
        }

    }
}
void Atualiza(int numero){
    char info[10];
    char msg[20];
    if (numero == CAPACIDADE){ //Cor vermlha
        gpio_put(LED_RED,1);
        gpio_put(LED_BLUE,0);
        gpio_put(LED_GREEN,0);
        snprintf(msg, sizeof(msg), "LOTADO!",CAPACIDADE);

    }else if(numero == CAPACIDADE-1){ //Cor Amarela
        gpio_put(LED_RED,1);
        gpio_put(LED_BLUE,0);
        gpio_put(LED_GREEN,1);
        snprintf(msg, sizeof(msg), "1 VAGA!",CAPACIDADE);

    }else if(numero > 0){ //Cor verde
        gpio_put(LED_RED,0);
        gpio_put(LED_BLUE,0);
        gpio_put(LED_GREEN,1);
        snprintf(msg, sizeof(msg), "DISPONIVEL",CAPACIDADE);

    }else{ //Cor azul
        gpio_put(LED_RED,0);
        gpio_put(LED_BLUE,1);
        gpio_put(LED_GREEN,0);
        snprintf(msg, sizeof(msg), "VAZIO",CAPACIDADE);
    }
    snprintf(info, sizeof(info), "%d / %d", numero,CAPACIDADE);
    ssd1306_fill(&ssd, false);
    ssd1306_rect(&ssd, 1, 1, 126, 62, 1, 1); // Borda grossa
    ssd1306_rect(&ssd, 4, 4, 120, 56, 0, 1); // Borda grossa
    ssd1306_draw_string(&ssd, "CONTROLE", 25, 7);
    ssd1306_draw_string(&ssd, "CAPACIDADE", 20, 17);
    ssd1306_draw_string(&ssd, info, 40, 27);
    ssd1306_draw_string(&ssd, msg, 15, 40);
    ssd1306_send_data(&ssd);
}
void PlayBuzzer(uint16_t warp){
    uint slice = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_wrap(slice, warp);
    pwm_set_gpio_level(BUZZER, warp/2);
    vTaskDelay(pdMS_TO_TICKS(200));
    pwm_set_gpio_level(BUZZER, 0); 

}
void vTaskEntrada(void *params){

    while(true){
        //Verifica se o semaforo binario de entrada está liberado
        if(xSemaphoreTake(xBinSemphEntrada,portMAX_DELAY) == pdTRUE){
            if(xSemaphoreGive(xContadorSemph) == pdTRUE){ //Se não está cheio coloca mais um carro
                BaseType_t contador = uxSemaphoreGetCount(xContadorSemph);
                if(xSemaphoreTake(xMutexDisplay, portMAX_DELAY) == pdTRUE){ //Se o display está disponível
                Atualiza(contador); //Atualiza display
                xSemaphoreGive(xMutexDisplay);} // Libera mutex
            }
            else{Atualiza(CAPACIDADE); //Se está cheio, atualiza com capcidade maxima e emite beep
            PlayBuzzer(1000);} //Aciona buzzer
        }vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}
void vTaskSaida(void *params){

    while(true){
        //Verifica se o semaforo binario de saída está liberado
        if(xSemaphoreTake(xBinSemphSaida,portMAX_DELAY) == pdTRUE){
            if(xSemaphoreTake(xContadorSemph,0) == pdTRUE){ //Se não está vazio tira um carro
                BaseType_t contador = uxSemaphoreGetCount(xContadorSemph); //Adciona uma vaga LIVRE
                if(xSemaphoreTake(xMutexDisplay, portMAX_DELAY) == pdTRUE){//Se o display está disponível
                Atualiza(contador); //Atualiza display
                xSemaphoreGive(xMutexDisplay);} // Libera mutex
            }
            else{Atualiza(0);} //Se está vazio atualiza com 0
        }vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}
void vTaskReset(void *params){

    while(true){
        //Verifica se o semaforo binario de reset está liberado
    if(xSemaphoreTake(xBinSemphReset,portMAX_DELAY) == pdTRUE){
        PlayBuzzer(500);
        vTaskDelay(pdMS_TO_TICKS(200));
        PlayBuzzer(500);
        vSemaphoreDelete(xContadorSemph);
        xContadorSemph = xSemaphoreCreateCounting(CAPACIDADE, 0);
        if(xSemaphoreTake(xMutexDisplay, portMAX_DELAY) == pdTRUE){ //Se o display está disponível
                Atualiza(0); //Atualiza display
                xSemaphoreGive(xMutexDisplay);} // Libera mutex
            }
    }}
