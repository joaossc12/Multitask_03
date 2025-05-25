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

#define CAPACIDADE 15

ssd1306_t ssd;
SemaphoreHandle_t xContadorSemph;
SemaphoreHandle_t xMutexDisplay;
SemaphoreHandle_t xBinSemph;
uint16_t contador = 0;

// ISR do botão A (incrementa o semáforo de contagem)
void gpio_callback(uint gpio, uint32_t events);
// ISR para BOOTSEL e botão de evento
void gpio_irq_handler(uint gpio, uint32_t events);

void Atualiza(int numero, char *msg){
    char info[10];

    snprintf(info, sizeof(info), "%d / %d", numero,CAPACIDADE);
    ssd1306_fill(&ssd, false);
    ssd1306_rect(&ssd, 1, 1, 126, 62, 1, 1); // Borda grossa
    ssd1306_rect(&ssd, 4, 4, 120, 56, 0, 1); // Borda grossa
    ssd1306_draw_string(&ssd, "CONTROLE", 25, 7);
    ssd1306_draw_string(&ssd, "CAPACIDADE", 20, 17);
    ssd1306_draw_string(&ssd, info, 40, 27);
    ssd1306_draw_string(&ssd, msg, 15, 37);
    ssd1306_send_data(&ssd);
}
void LedAtt(int numero){
    if (numero == CAPACIDADE){ //Cor vermlha
        gpio_put(LED_RED,1);
        gpio_put(LED_BLUE,0);
        gpio_put(LED_GREEN,0);
    }else if(numero == CAPACIDADE-1){ //Cor Amarela
        gpio_put(LED_RED,1);
        gpio_put(LED_BLUE,0);
        gpio_put(LED_GREEN,1);

    }else if(numero > 0){ //Cor verde
        gpio_put(LED_RED,0);
        gpio_put(LED_BLUE,0);
        gpio_put(LED_GREEN,1);

    }else{ //Cor azul
        gpio_put(LED_RED,0);
        gpio_put(LED_BLUE,1);
        gpio_put(LED_GREEN,0);
    }

}

void PlayBuzzer(uint16_t warp){
    uint slice = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_wrap(slice, warp);
    pwm_set_gpio_level(BUZZER, warp/2);
    vTaskDelay(pdMS_TO_TICKS(200));
    pwm_set_gpio_level(BUZZER, 0); 

}

void vTaskEntrada(void *params){
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    while(true){
        if(!gpio_get(BOTAO_A)){
            if(xSemaphoreTake(xContadorSemph,0) == pdTRUE){ //Se tem vaga disponível consome uma vaga
                contador++; //acresce o contador
                if(xSemaphoreTake(xMutexDisplay, portMAX_DELAY) == pdTRUE){ //Se o display está disponível
                Atualiza(contador, "Disponivel"); //Atualiza display
                LedAtt(contador); //Atualiza cor do led
                xSemaphoreGive(xMutexDisplay);} // Libera mutex
            }
            else{Atualiza(contador, "LOTADO"); //Se não tem vaga disponivel exibe LOTADO
            PlayBuzzer(1000);} //Aciona buzzer
        }vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}

void vTaskSaida(void *params){
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

    while(true){
        if(!gpio_get(BOTAO_B)){
            if(xSemaphoreGive(xContadorSemph) == pdTRUE){ //Se não está vazio
                contador--; //Decresce uma vaga
                if(xSemaphoreTake(xMutexDisplay, portMAX_DELAY) == pdTRUE){//Se o display está disponível
                Atualiza(contador, "Disponivel"); //Atualiza display
                LedAtt(contador); //Atualiza cor do led
                xSemaphoreGive(xMutexDisplay);} // Libera mutex
            }
            else{Atualiza(contador, "VAZIO");} //Se está vazio exibe VAZIO
        }vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}

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

    gpio_set_irq_enabled_with_callback(JOYSTICK_BT, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

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

    Atualiza(contador, "Disponivel"); //Atualiza display
    LedAtt(contador);

    // Cria semáforo de contagem (máximo 10, inicial 0)
    xContadorSemph = xSemaphoreCreateCounting(CAPACIDADE, CAPACIDADE);
    xMutexDisplay = xSemaphoreCreateMutex();
    xBinSemph = xSemaphoreCreateBinary();

    xTaskCreate(vTaskEntrada, "Task Entrada", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskSaida, "Task Saida", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

    vTaskStartScheduler();
    panic_unsupported();

}
// ISR do botão A (incrementa o semáforo de contagem)
void gpio_callback(uint gpio, uint32_t events)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xContadorSemph, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
// ISR para BOOTSEL e botão de evento
void gpio_irq_handler(uint gpio, uint32_t events)
{
    static absolute_time_t last_time_JB = 0;
    absolute_time_t now = get_absolute_time();
    if (gpio == JOYSTICK_BT)
    {
        if (absolute_time_diff_us(last_time_JB, now) > 200000) { //Deboucing
            reset_usb_boot(0, 0);
            last_time_JB = now;
        }
    }
}