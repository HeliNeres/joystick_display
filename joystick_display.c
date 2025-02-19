#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "pico/bootrom.h"
#include "hardware/pwm.h" //biblioteca para controlar o hardware de PWM

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define JOYSTICK_X_PIN 26  // GPIO para eixo X
#define JOYSTICK_Y_PIN 27  // GPIO para eixo Y
#define JOYSTICK_PB 22 // GPIO para botão do Joystick
#define Botao_A 5 // GPIO para botão A
#define Botao_B 6

const uint LED_G = 11;
const uint LED_B = 12;
const uint LED_R = 13;

const float xmax = 4090.0;
const float xmin = 22.0;

const float ymax = 4087.0;
const float ymin = 22.0;

const float ypixels = 128.0;
const float xpixels = 64.0;

bool pwm_status = true;
bool green_status = false;
uint last_time_A = 0;
uint last_time_PB = 0;

void inicializar();
void gpio_irq_handler(uint gpio, uint32_t events);
void inicializar_pmw(uint pin);
double normal(uint valor_adc);

int main()
{    
    inicializar();
    inicializar_pmw(LED_B);
    inicializar_pmw(LED_R);

    ssd1306_t ssd; // Inicializa a estrutura do display
  
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display
  
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
  
    uint adc_value_x;
    uint adc_value_y;

    uint x;
    uint y;

    double x_normal = 0;
    double y_normal = 0;

    char str_x[5];  // Buffer para armazenar a string
    char str_y[5];  // Buffer para armazenar a string  
    
    bool cor = true;
    while (true)
    {
        adc_select_input(0); // Seleciona o ADC para eixo X. O pino 26 como entrada analógica
        adc_value_x = adc_read();
        x_normal = normal(adc_value_x);
        adc_select_input(1); // Seleciona o ADC para eixo Y. O pino 27 como entrada analógica
        adc_value_y = adc_read();
        y_normal = normal(adc_value_y);
        //printf("X: %d Y: %d | Xn: %f Yn: %f\n", adc_value_x,adc_value_y,x_normal,y_normal);
        sprintf(str_x, "%d", adc_value_x);  // Converte o inteiro em string
        sprintf(str_y, "%d", adc_value_y);  // Converte o inteiro em string
        
        //cor = !cor;
        // Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor); // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
        ssd1306_rect(&ssd, 0, 0, 128, 64, !green_status, !cor); // Desenha um retângulo
        
        x=adc_value_x/4095;
        y=adc_value_y/4095;

        printf("X: %d Y: %d | Xn: %f Yn: %f\n", (x)*128-4,(y)*64-4,(x_normal)*128-4,(y_normal)*64-4);

        ssd1306_rect(&ssd, (x)*64-4, (y)*128-4, 8, 8, cor, cor); // Desenha um retângulo
        //ssd1306_rect(&ssd, (x_normal)*64-4, (y_normal)*128-4, 8, 8, cor, cor); // Desenha um retângulo
        ssd1306_send_data(&ssd); // Atualiza o display

        pwm_set_gpio_level(LED_B, adc_value_y);
        pwm_set_gpio_level(LED_R, adc_value_x);

        sleep_ms(100);
    }

    return 0;
}

void gpio_irq_handler(uint gpio, uint32_t events){// Para ser utilizado o modo BOOTSEL com botão B
    if(gpio==Botao_A){
        if(absolute_time_diff_us(last_time_A,get_absolute_time()) > 300000){ //debouncing de 200ms
            pwm_status = !pwm_status;

            pwm_set_gpio_level(LED_B, 0);
            pwm_set_gpio_level(LED_R, 0);
            
            uint slice = pwm_gpio_to_slice_num(LED_B);
            pwm_set_enabled(slice, pwm_status);
            slice = pwm_gpio_to_slice_num(LED_R);
            pwm_set_enabled(slice, pwm_status);

            last_time_A = get_absolute_time();
        }
    }else if(gpio==JOYSTICK_PB){
        if(absolute_time_diff_us(last_time_PB,get_absolute_time()) > 200000){ //debouncing de 200ms
            green_status = !green_status;

            gpio_put(LED_G,green_status);

            last_time_PB = get_absolute_time();
        }
    }else{
        reset_usb_boot(0, 0);
    }
}

void inicializar(){
    stdio_init_all(); //inicializa o sistema padrão de I/O

    // inicializar led verde
    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_put(LED_G,0);

    gpio_init(Botao_B);
    gpio_set_dir(Botao_B, GPIO_IN);
    gpio_pull_up(Botao_B);
    gpio_set_irq_enabled_with_callback(Botao_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    gpio_init(JOYSTICK_PB);
    gpio_set_dir(JOYSTICK_PB, GPIO_IN);
    gpio_pull_up(JOYSTICK_PB); 
    gpio_set_irq_enabled_with_callback(JOYSTICK_PB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    gpio_init(Botao_A);
    gpio_set_dir(Botao_A, GPIO_IN);
    gpio_pull_up(Botao_A);
    gpio_set_irq_enabled_with_callback(Botao_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA); // Pull up the data line
    gpio_pull_up(I2C_SCL); // Pull up the clock line

    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);  
  
}

void inicializar_pmw(uint pin){
    const float divisor = 125.0;
    uint16_t wrap = 4095;
    uint nivel = 0;

    gpio_set_function(pin, GPIO_FUNC_PWM); //habilitar o pino GPIO como PWM

    uint slice = pwm_gpio_to_slice_num(pin); //obter o canal PWM da GPIO

    pwm_set_clkdiv(slice, divisor); //define o divisor de clock do PWM
    pwm_set_wrap(slice, wrap); //definir o valor de wrap
    pwm_set_gpio_level(pin, nivel); //definir o ciclo de trabalho (duty cycle) do pwm
    pwm_set_enabled(slice, true); //habilita o pwm no slice correspondente
}

double normal(uint valor_adc){
    double valor = 0;
    if(valor_adc>2048){
        valor = (valor_adc - 2048.0)/2048.0;
    }else{
        valor = (2048.0 - valor_adc)/2048.0;
    }

    return valor;
}