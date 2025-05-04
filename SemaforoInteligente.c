#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "pico/bootrom.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "ws2812.pio.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include <stdio.h>
#include <stdbool.h>

// Definições de pinos
#define BUZZER_PIN     10
#define WS2812_PIN     7
#define BOTAO_A        5
#define BOTAO_B        6

// Definições do LED RGB
#define LED_G          11  // Pino do LED verde
#define LED_B          12  // Pino do LED azul
#define LED_R          13  // Pino do LED vermelho

// Definições do display
#define I2C_PORT       i2c1
#define I2C_SDA        14
#define I2C_SCL        15
#define DISPLAY_ADDR   0x3C

// Definições da matriz de LEDs
#define NUM_PIXELS     25
#define BRILHO_LED     10  // Intensidade do brilho (0-255)

// Cores para a matriz de LEDs
#define COR_VERDE      0, BRILHO_LED, 0
#define COR_AMARELO    BRILHO_LED, BRILHO_LED, 0
#define COR_VERMELHO   BRILHO_LED, 0, 0
#define COR_DESLIGADO  0, 0, 0

// Tempos em milissegundos
#define TEMPO_VERDE        15000        // 15 segundos para luz verde
#define TEMPO_AMARELO      5000         // 5 segundos para luz amarela
#define TEMPO_VERMELHO     15000        // 15 segundos para luz vermelha
#define TEMPO_BEEP         500          // Duração do beep
#define TEMPO_PISCA        2000         // Tempo de pisca no modo noturno
#define TEMPO_AVISO        3000         // Tempo de aviso antes da troca (3 segundos)
#define TEMPO_PISCA_RAPIDO 100          // Tempo de pisca rápido para o LED RGB (100ms)

// Estados do semáforo
#define ESTADO_VERDE    0
#define ESTADO_AMARELO  1
#define ESTADO_VERMELHO 2

// Variáveis globais compartilhadas
volatile bool modo_noturno = false;             // Flag para controle do modo
volatile int estado_semaforo = 0;               // 0=verde, 1=amarelo, 2=vermelho
volatile bool led_aceso = false;                // Flag para sincronizar LED e buzzer no modo noturno
volatile uint32_t tempo_atual = 0;              // Tempo atual do estado do semáforo
volatile uint32_t tempo_estado = 0;             // Tempo total do estado atual
volatile uint32_t timestamp_modo_noturno = 0;   // Timestamp para sincronização no modo noturno
volatile bool aviso_troca = false;              // Flag para indicar que está próximo da troca de estado

// Variáveis para o PIO da matriz de LEDs
PIO pio = pio0;
int sm = 0;

// Protótipos de funções
void init_hardware();
void init_buzzer();
void init_matriz_leds();
void init_display();
void init_led_rgb();
uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
void put_pixel(uint32_t pixel_grb);
void display_matriz(uint8_t r, uint8_t g, uint8_t b);
void led_rgb_controller(bool r, bool g, bool b);

// Tarefas FreeRTOS
void vSemaforoControleTask(void *pvParameters);
void vMatrizLedsTask(void *pvParameters);
void vDisplayTask(void *pvParameters);
void vBuzzerTask(void *pvParameters);
void vBotoesTask(void *pvParameters);
void vLedRgbTask(void *pvParameters);

// Função para modo BOOTSEL com botão B
void gpio_irq_handler(uint gpio, uint32_t events) {
    if (gpio == BOTAO_B) {
        reset_usb_boot(0, 0);
    }
}

// Inicialização do hardware
void init_hardware() {
    
    init_buzzer();          // Inicializa o buzzer
    init_matriz_leds();     // Inicializa a matriz de LEDs
    init_display();         // Inicializa o display
    init_led_rgb();         // Inicializa o LED RGB
    
    // Configura o botão A
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    
    // Configura o botão B para modo BOOTSEL
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
}

// Inicializa o buzzer
void init_buzzer() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(BUZZER_PIN, 0);
}

// Inicializa a matriz de LEDs
void init_matriz_leds() {
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, false);
    
    display_matriz(0, 0, 0); // Limpa a matriz
}

// Inicializa o display OLED
void init_display() {
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

// Inicializa o LED RGB
void init_led_rgb() {
    // Inicializa os pinos do LED RGB
    gpio_init(LED_R);
    gpio_init(LED_G);
    gpio_init(LED_B);
    
    // Configura os pinos como saída
    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_set_dir(LED_B, GPIO_OUT);
    
    // Inicialmente, todos os LEDs estão desligados
    gpio_put(LED_R, 0);
    gpio_put(LED_G, 0);
    gpio_put(LED_B, 0);
}

// Controla o LED RGB
void led_rgb_controller(bool r, bool g, bool b) {
    gpio_put(LED_R, r);
    gpio_put(LED_G, g);
    gpio_put(LED_B, b);
}

// Converte componentes RGB em GRB para o WS2812
uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(g) << 16) | ((uint32_t)(r) << 8) | (uint32_t)(b);
}

// Envia um pixel para o PIO
void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

// Exibe um frame completo na matriz de LEDs
void display_matriz(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = urgb_u32(r, g, b);
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(color);
    }
}

// Tarefa para controle da lógica do semáforo
void vSemaforoControleTask(void *pvParameters) {
    tempo_estado = TEMPO_VERDE;  // Inicia com o tempo do estado verde
    uint32_t ultimo_tick_modo_noturno = 0;
    
    while (true) {
        if (modo_noturno) {                     // Modo noturno: pisca amarelo lentamente
            estado_semaforo = ESTADO_AMARELO;
            aviso_troca = false;                // Não há aviso de troca no modo noturno
            
            // Calcula o tempo decorrido desde o último ciclo
            uint32_t tick_atual = xTaskGetTickCount();
            uint32_t tempo_decorrido = tick_atual - ultimo_tick_modo_noturno;
            
            // Verifica se é hora de alternar o estado do LED
            if (tempo_decorrido >= pdMS_TO_TICKS(TEMPO_PISCA)) {
                // Alterna entre LED aceso e apagado
                led_aceso = !led_aceso;
                ultimo_tick_modo_noturno = tick_atual;
                
                // Atualiza o timestamp para sincronização
                timestamp_modo_noturno = tick_atual;
            }
            
            // Pequeno delay para não sobrecarregar o processador
            vTaskDelay(pdMS_TO_TICKS(50));
        } else {
            // Modo normal: ciclo verde -> amarelo -> vermelho
            
            // Define o tempo total do estado atual
            switch (estado_semaforo) {
                case ESTADO_VERDE:
                    tempo_estado = TEMPO_VERDE;
                    break;
                case ESTADO_AMARELO:
                    tempo_estado = TEMPO_AMARELO;
                    break;
                case ESTADO_VERMELHO:
                    tempo_estado = TEMPO_VERMELHO;
                    break;
            }
            
            // Reinicia o tempo atual
            tempo_atual = 0;
            
            // Espera o tempo do estado atual
            while (tempo_atual < tempo_estado) {
                // Verifica se está próximo da troca de estado (faltando 3 segundos)
                if (tempo_atual >= (tempo_estado - TEMPO_AVISO)) {
                    aviso_troca = true;  // Ativa o aviso de troca
                } else {
                    aviso_troca = false;  // Desativa o aviso de troca
                }
                
                tempo_atual += 100;  // Incrementa em 100ms
                vTaskDelay(pdMS_TO_TICKS(100));
                
                // Verifica se houve mudança para modo noturno
                if (modo_noturno) {
                    aviso_troca = false;  // Desativa o aviso de troca
                    break;
                }
            }
            
            // Se completou o tempo e não está em modo noturno, avança para o próximo estado
            if (tempo_atual >= tempo_estado && !modo_noturno) {
                // Avança para o próximo estado
                estado_semaforo = (estado_semaforo + 1) % 3;
                aviso_troca = false;  // Desativa o aviso de troca
            }
        }
    }
}

// Tarefa para controle da matriz de LEDs
void vMatrizLedsTask(void *pvParameters) {
    while (true) {
        if (modo_noturno) {
            // Modo noturno: pisca amarelo lentamente
            if (led_aceso) {
                display_matriz(COR_AMARELO);
            } else {
                display_matriz(COR_DESLIGADO);
            }
        } else {
            // Modo normal: exibe a cor correspondente ao estado atual
            switch (estado_semaforo) {
                case ESTADO_VERDE:
                    display_matriz(COR_VERDE);
                    break;
                case ESTADO_AMARELO:
                    display_matriz(COR_AMARELO);
                    break;
                case ESTADO_VERMELHO:
                    display_matriz(COR_VERMELHO);
                    break;
            }
        }
        
        // Atualiza a cada 50ms para responder mais rapidamente
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Tarefa para controle do display
void vDisplayTask(void *pvParameters) {
    ssd1306_t display;
    ssd1306_init(&display, WIDTH, HEIGHT, false, DISPLAY_ADDR, I2C_PORT);
    ssd1306_config(&display);
    
    while (true) {
        ssd1306_fill(&display, false);  // Limpa o display
        
        if (modo_noturno) {
            // Modo noturno
            ssd1306_draw_string(&display, "MODO NOTURNO", 20, 20);
            ssd1306_draw_string(&display, "Atencao", 35, 40);
        } else {
            // Modo normal
            ssd1306_draw_string(&display, "SEMAFORO", 30, 10);
            
            switch (estado_semaforo) {
                case ESTADO_VERDE:
                    ssd1306_draw_string(&display, "Siga", 45, 30);
                    break;
                case ESTADO_AMARELO:
                    ssd1306_draw_string(&display, "Atencao", 35, 30);
                    break;
                case ESTADO_VERMELHO:
                    ssd1306_draw_string(&display, "Pare", 45, 30);
                    break;
            }
            
            // Se estiver próximo da troca, exibe uma mensagem adicional
            if (aviso_troca) {
                char tempo_restante[20];
                sprintf(tempo_restante, "Troca em %ds", (tempo_estado - tempo_atual) / 1000 + 1);
                ssd1306_draw_string(&display, tempo_restante, 20, 50);
            }
        }
        
        ssd1306_send_data(&display);
        vTaskDelay(pdMS_TO_TICKS(100));  // Atualiza o display a cada 100ms
    }
}

// Tarefa para controle do buzzer
void vBuzzerTask(void *pvParameters) {
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    float divider = 20.0f;
    uint16_t wrap;
    
    while (true) {
        if (modo_noturno) {
            // Modo noturno: beep lento a cada 2s
            // Verifica se o LED acabou de acender (usando o timestamp)
            uint32_t tick_atual = xTaskGetTickCount();
            uint32_t tempo_desde_mudanca = tick_atual - timestamp_modo_noturno;
            
            if (led_aceso && tempo_desde_mudanca < pdMS_TO_TICKS(200)) {
                // Emite o beep apenas quando o LED acaba de acender
                pwm_set_clkdiv(slice_num, divider);
                wrap = (125000000 / (800 * divider)) - 1;
                pwm_set_wrap(slice_num, wrap);
                pwm_set_gpio_level(BUZZER_PIN, wrap / 2);
                
                // Toca por 200ms
                vTaskDelay(pdMS_TO_TICKS(200));
                pwm_set_gpio_level(BUZZER_PIN, 0);
            } else {
                // Mantém o buzzer desligado e verifica frequentemente
                pwm_set_gpio_level(BUZZER_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        } else {
            // Modo normal: diferentes sons para cada cor
            switch (estado_semaforo) {
                case ESTADO_VERDE:
                    // Verde - 1 beep curto por segundo
                    pwm_set_clkdiv(slice_num, divider);
                    wrap = (125000000 / (1000 * divider)) - 1;
                    pwm_set_wrap(slice_num, wrap);
                    pwm_set_gpio_level(BUZZER_PIN, wrap / 2);
                    
                    vTaskDelay(pdMS_TO_TICKS(100));  // Beep curto
                    pwm_set_gpio_level(BUZZER_PIN, 0);
                    vTaskDelay(pdMS_TO_TICKS(900));  // Espera para completar 1s
                    break;
                    
                case ESTADO_AMARELO:
                    // Amarelo - beep rápido intermitente
                    pwm_set_clkdiv(slice_num, divider);
                    wrap = (125000000 / (1200 * divider)) - 1;
                    pwm_set_wrap(slice_num, wrap);
                    pwm_set_gpio_level(BUZZER_PIN, wrap / 2);
                    
                    vTaskDelay(pdMS_TO_TICKS(100));  // Beep curto
                    pwm_set_gpio_level(BUZZER_PIN, 0);
                    vTaskDelay(pdMS_TO_TICKS(100));  // Espera curta
                    break;
                    
                case ESTADO_VERMELHO:
                    // Vermelho - tom contínuo curto (500ms ligado, 1.5s desligado)
                    pwm_set_clkdiv(slice_num, divider);
                    wrap = (125000000 / (1500 * divider)) - 1;
                    pwm_set_wrap(slice_num, wrap);
                    pwm_set_gpio_level(BUZZER_PIN, wrap / 2);
                    
                    vTaskDelay(pdMS_TO_TICKS(500));  // Beep de 500ms
                    pwm_set_gpio_level(BUZZER_PIN, 0);
                    vTaskDelay(pdMS_TO_TICKS(1500));  // Espera de 1.5s
                    break;
            }
        }
    }
}

// Tarefa para controle do LED RGB
void vLedRgbTask(void *pvParameters) {
    bool led_rgb_estado = false;  // Estado do LED RGB (ligado/desligado)
    
    while (true) {
        if (modo_noturno) {
            // No modo noturno, o LED RGB permanece desligado
            led_rgb_controller(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else if (aviso_troca) {
            // Se estiver próximo da troca de estado, pisca o LED RGB na cor correspondente ao estado ATUAL
            led_rgb_estado = !led_rgb_estado;  // Alterna o estado do LED RGB
            
            switch (estado_semaforo) {
                case ESTADO_VERDE:
                    // Sinal em VERDE com o tempo acabando - pisca em VERDE
                    led_rgb_controller(0, led_rgb_estado, 0);
                    break;
                case ESTADO_AMARELO:
                    // Sinal em AMARELO com o tempo acabando - pisca em AMARELO
                    led_rgb_controller(led_rgb_estado, led_rgb_estado, 0);
                    break;
                case ESTADO_VERMELHO:
                    // Sinal em VERMELHO com o tempo acabando - pisca em VERMELHO
                    led_rgb_controller(led_rgb_estado, 0, 0);
                    break;
            }
            
            // Pisca rapidamente (100ms ligado, 100ms desligado)
            vTaskDelay(pdMS_TO_TICKS(TEMPO_PISCA_RAPIDO));
        } else {
            // Se não estiver próximo da troca, o LED RGB permanece desligado
            led_rgb_controller(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(100));  // Verifica a cada 100ms
        }
    }
}

// Tarefa para monitorar os botões
void vBotoesTask(void *pvParameters) {
    bool ultimo_estado_botao_a = true;  // Pull-up, então começa HIGH
    
    while (true) {
        // Lê o estado atual do botão A
        bool estado_atual_botao_a = gpio_get(BOTAO_A);
        
        // Detecta borda de descida (botão pressionado)
        if (ultimo_estado_botao_a && !estado_atual_botao_a) {
            // Alterna o modo
            bool modo_anterior = modo_noturno;
            modo_noturno = !modo_noturno;
            
            // Se estiver saindo do modo noturno, reinicia no estado VERDE
            if (modo_anterior && !modo_noturno) {
                estado_semaforo = ESTADO_VERDE;
                tempo_atual = 0;
            }
            
            // Reinicia o estado do LED e o timestamp
            led_aceso = false;
            timestamp_modo_noturno = xTaskGetTickCount();
            
            printf("Modo alterado: %s\n", modo_noturno ? "Noturno" : "Normal");
            if (!modo_noturno) {
                printf("Reiniciando no estado VERDE\n");
            }
            
            // Pequeno atraso para debounce
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        
        ultimo_estado_botao_a = estado_atual_botao_a;
        vTaskDelay(pdMS_TO_TICKS(50));  // Verifica a cada 50ms
    }
}

int main() {
    // Inicialização do hardware
    stdio_init_all();
    init_hardware();
    
    // Criação das tarefas
    xTaskCreate(
        vSemaforoControleTask,
        "Controle Semaforo",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 3,
        NULL
    );
    
    xTaskCreate(
        vMatrizLedsTask,
        "Matriz LEDs",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
    
    xTaskCreate(
        vDisplayTask,
        "Display",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
    
    xTaskCreate(
        vBuzzerTask,
        "Buzzer",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
    
    xTaskCreate(
        vLedRgbTask,
        "LED RGB",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
    
    xTaskCreate(
        vBotoesTask,
        "Botoes",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 2,
        NULL
    );
    
    // Inicia o escalonador
    vTaskStartScheduler();
    
    // Nunca deve chegar aqui se o escalonador estiver funcionando
    panic_unsupported();
    return 0;
}