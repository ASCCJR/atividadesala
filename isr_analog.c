#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "pico/time.h" // Incluir para usar absolute_time_t e funções de tempo

// Definições de pinos e canais ADC
#define JOY_Y_ADC_CHANNEL 1     // Canal ADC 1 está conectado ao GPIO27, que é o eixo Y do joystick
#define JOY_X_ADC_CHANNEL 0     // Canal ADC 0 está conectado ao GPIO26, que é o eixo X do joystick
#define LED_VERMELHO 13         // GPIO13 conectado ao LED Vermelho
#define LED_VERDE 11            // GPIO11 conectado ao LED Verde
#define LED_AZUL 12             // GPIO12 conectado ao LED Azul
#define BUZZER_PIN 21           // GPIO21 conectado ao buzzer

// Usamos GPIO21 porque ele possui funcionalidade PWM (Pulse Width Modulation)
// necessária para controlar o volume do buzzer, e está conectado através de um
// transistor que atua como uma chave para controlar a corrente que passa pelo buzzer.

// Definições de tempo e limites
// ✅ Requisito atendido: Ajustar o limite de acionamento. Estes defines controlam os pontos de ativação.
#define INTERVALO_MS 50         // Intervalo de tempo em milissegundos entre cada leitura do joystick
#define LIMITE_ACIONAMENTO_Y_UP 3500    // Valor limite de leitura do ADC para considerar o joystick pressionado para cima no eixo Y
#define LIMITE_ACIONAMENTO_Y_DOWN 1000  // Valor limite de leitura do ADC para considerar o joystick pressionado para baixo no eixo Y
#define LIMITE_ACIONAMENTO_X_RIGHT 3500 // Valor limite de leitura do ADC para considerar o joystick pressionado para a direita no eixo X
#define DURACAO_BUZZER_MS 300   // Duração em milissegundos que o buzzer soará após uma detecção de movimento

// Definições de volume do buzzer (controlado pelo duty cycle do PWM, escala de 0 a 255)
#define VOLUME_ALTO 220         // Duty cycle alto para um volume mais alto
#define VOLUME_MEDIO 160        // Duty cycle médio para um volume médio
#define VOLUME_BAIXO 100        // Duty cycle baixo para um volume mais baixo

// Definições de frequência do buzzer (em Hertz)
#define FREQ_CIMA 3000          // Frequência mais alta para um tom agudo (associado ao movimento para cima)
#define FREQ_BAIXO 1500         // Frequência média para um tom médio (associado ao movimento para baixo)
#define FREQ_DIREITA 800        // Frequência mais baixa para um tom grave (associado ao movimento para a direita)

// --- NOVAS DEFINIÇÕES PARA REQUISITOS ADICIONAIS ---
// ✅ Requisito atendido: Implementar uma média móvel de 10 leituras. Define o tamanho da janela.
#define MOVING_AVERAGE_SIZE 10  // Tamanho da janela da média móvel
// ✅ Requisito atendido: Exibir uma barra gráfica proporcional ao valor do ADC no terminal. Define a largura.
#define BAR_GRAPH_WIDTH 50      // Largura máxima da barra gráfica no terminal
// ✅ Requisito atendido: Fazer o LED piscar ao invés de acender continuamente. Define a velocidade do piscar.
#define BLINK_INTERVAL_MS 200   // Intervalo de tempo em milissegundos para o LED piscar

// Variáveis globais para armazenar o número da 'slice' PWM
uint slice_num, channel;

// --- NOVAS VARIÁVEIS PARA REQUISITOS ADICIONAIS ---
// ✅ Requisito atendido: Implementar uma média móvel de 10 leituras. Buffers e variáveis de controle para a média.
uint16_t y_readings[MOVING_AVERAGE_SIZE]; // Buffer para leituras do eixo Y
uint16_t x_readings[MOVING_AVERAGE_SIZE]; // Buffer para leituras do eixo X
uint32_t y_sum = 0; // Soma das leituras do eixo Y no buffer
uint32_t x_sum = 0; // Soma das leituras do eixo X no buffer
uint8_t reading_index = 0; // Índice atual no buffer (circular)
uint8_t reading_count = 0; // Número de leituras válidas no buffer (para inicialização)

// ✅ Requisito atendido: Fazer o LED piscar ao invés de acender continuamente. Variáveis de estado e tempo para controlar o piscar.
bool led_vermelho_on_state = false; // Estado atual do LED Vermelho para piscar (true=ligado, false=desligado)
bool led_verde_on_state = false;    // Estado atual do LED Verde para piscar
bool led_azul_on_state = false;     // Estado atual do LED Azul para piscar
absolute_time_t last_blink_time_vermelho; // Último tempo que o LED Vermelho mudou de estado para piscar
absolute_time_t last_blink_time_verde;    // Último tempo que o LED Verde mudou de estado para piscar
absolute_time_t last_blink_time_azul;     // Último tempo que o LED Azul mudou de estado para piscar


// Função para configurar o PWM no pino conectado ao buzzer.
void configurar_pwm_buzzer() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    channel = pwm_gpio_to_channel(BUZZER_PIN);
    pwm_set_chan_level(slice_num, channel, 0);
}

// Função para ativar o buzzer com um determinado volume e frequência.
void ativar_buzzer(uint16_t volume, uint32_t frequencia) {
    uint32_t clock = 125000000;
    float divisor = 16.0;
    uint32_t wrap = clock / divisor / frequencia;

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, divisor);
    pwm_config_set_wrap(&config, wrap);
    pwm_init(slice_num, &config, true);

    // Escala o volume (0-255) para a escala do wrap (0-wrap) para definir o duty cycle
     uint16_t duty_cycle = (uint16_t)((float)volume / 255.0 * wrap);
    pwm_set_chan_level(slice_num, channel, duty_cycle);
}

// Função para desativar o buzzer.
void desativar_buzzer() {
     pwm_set_chan_level(slice_num, channel, 0);
}

// ✅ Requisito atendido: Implementar uma média móvel de 10 leituras. Função para adicionar leitura e atualizar a soma.
void update_moving_average(uint16_t raw_y, uint16_t raw_x) {
    if (reading_count < MOVING_AVERAGE_SIZE) {
        // Buffer ainda não cheio
        y_readings[reading_index] = raw_y;
        x_readings[reading_index] = raw_x;
        y_sum += raw_y;
        x_sum += raw_x;
        reading_count++;
    } else {
        // Buffer cheio: remove a leitura mais antiga e adiciona a nova
        y_sum -= y_readings[reading_index]; // Remove o valor antigo da soma
        x_sum -= x_readings[reading_index];
        y_readings[reading_index] = raw_y;  // Armazena o novo valor
        x_readings[reading_index] = raw_x;
        y_sum += raw_y;                     // Adiciona o novo valor à soma
        x_sum += raw_x;
    }
    // Move para o próximo índice, circulando
    reading_index = (reading_index + 1) % MOVING_AVERAGE_SIZE;
}

// ✅ Requisito atendido: Implementar uma média móvel de 10 leituras. Função para calcular a média.
uint16_t get_average(uint32_t sum, uint8_t count) {
    if (count == 0) return 0; // Evita divisão por zero
    return (uint16_t)(sum / count);
}

// ✅ Requisito atendido: Exibir uma barra gráfica proporcional ao valor do ADC no terminal. Função para gerar e imprimir a barra.
void print_bar_graph(uint16_t value, const char* label) {
    // Escala o valor (0-4095) para a largura da barra (0-BAR_GRAPH_WIDTH)
    int bar_length = (value * BAR_GRAPH_WIDTH) / 4095;

    printf("%s [%4u]: [", label, value); // Imprime o rótulo e o valor numérico
    for (int i = 0; i < BAR_GRAPH_WIDTH; i++) {
        if (i < bar_length) {
            printf("#"); // Imprime a parte preenchida da barra
        } else {
            printf(" "); // Imprime a parte vazia da barra
        }
    }
    printf("]\n"); // Fecha a barra e adiciona uma nova linha
}


int main() {
    // Inicializa a comunicação serial
    stdio_init_all();
    sleep_ms(3000); // Pequena pausa para iniciar a serial
    printf("Sistema Joystick com Volume e Frequencia Variaveis\n");

    // Inicializa os pinos dos LEDs como saídas
    gpio_init(LED_VERMELHO);
    gpio_init(LED_VERDE);
    gpio_init(LED_AZUL);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
    // Desliga todos os LEDs inicialmente (saída em 1 para LED ativo baixo)
    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 1);
    gpio_put(LED_AZUL, 1);

    // ✅ Requisito atendido: Fazer o LED piscar ao invés de acender continuamente. Inicializa os tempos de último blink.
    last_blink_time_vermelho = get_absolute_time();
    last_blink_time_verde = get_absolute_time();
    last_blink_time_azul = get_absolute_time();


    // Configura o PWM para controlar o buzzer
    configurar_pwm_buzzer();

    // Inicializa o ADC (Analog-to-Digital Converter)
    adc_init();
    // ✅ Requisito atendido: Usar outro eixo do joystick. Inicializa os pinos GPIO para ambos os eixos (X e Y).
    adc_gpio_init(26); // GPIO26 para o eixo X (ADC0)
    adc_gpio_init(27); // GPIO27 para o eixo Y (ADC1)

     // ✅ Requisito atendido: Implementar uma média móvel de 10 leituras. Preenche o buffer inicial da média móvel.
     // Isso evita que a média seja calculada sobre zero leituras no início.
     for(int i = 0; i < MOVING_AVERAGE_SIZE; i++) {
         adc_select_input(JOY_Y_ADC_CHANNEL);
         uint16_t raw_y = adc_read();
         adc_select_input(JOY_X_ADC_CHANNEL);
         uint16_t raw_x = adc_read();
         update_moving_average(raw_y, raw_x);
         sleep_ms(10); // Pequeno delay entre as leituras iniciais
     }


    // Variável para armazenar o tempo em que o buzzer deve ser desligado.
    absolute_time_t buzzer_off_time = nil_time;

    // Loop principal do programa
    while (true) {
        // ✅ Requisito atendido: Usar outro eixo do joystick. Lê valores RAW de ambos os eixos.
        // ✅ Requisito atendido: Implementar uma média móvel de 10 leituras. Leituras RAW usadas para alimentar a média.
        adc_select_input(JOY_Y_ADC_CHANNEL); // Seleciona o canal Y
        uint16_t raw_y = adc_read();         // Lê o valor RAW do eixo Y
        adc_select_input(JOY_X_ADC_CHANNEL); // Seleciona o canal X
        uint16_t raw_x = adc_read();         // Lê o valor RAW do eixo X

        // ✅ Requisito atendido: Implementar uma média móvel de 10 leituras. Atualiza os buffers e somas com as novas leituras RAW.
        update_moving_average(raw_y, raw_x);

        // ✅ Requisito atendido: Implementar uma média móvel de 10 leituras. Calcula os valores médios suavizados.
        uint16_t avg_y = get_average(y_sum, reading_count);
        uint16_t avg_x = get_average(x_sum, reading_count);

        // ✅ Requisito atendido: Exibir uma barra gráfica proporcional ao valor do ADC no terminal. Imprime as barras usando os valores médios.
        print_bar_graph(avg_y, "Eixo Y");
        print_bar_graph(avg_x, "Eixo X");
        printf("\n"); // Linha em branco para separar as leituras no terminal

        // Verifica se o tempo para desligar o buzzer já passou
        if (!is_nil_time(buzzer_off_time) && time_reached(buzzer_off_time)) {
            desativar_buzzer();          // Desativa o buzzer
            buzzer_off_time = nil_time;  // Reseta o tempo de desligamento
        }

        // Obtém o tempo atual para a lógica de piscar do LED e controle do buzzer
        absolute_time_t current_time = get_absolute_time();

        // ✅ Requisito atendido: Ajustar o limite de acionamento. As condições usam os limites definidos.
        // ✅ Requisito atendido: Implementar uma média móvel de 10 leituras. As condições usam os valores MÉDIOS.
        bool condition_y_up = avg_y > LIMITE_ACIONAMENTO_Y_UP;
        bool condition_y_down = avg_y < LIMITE_ACIONAMENTO_Y_DOWN;
        bool condition_x_right = avg_x > LIMITE_ACIONAMENTO_X_RIGHT;

        // --- Lógica de controle para Y_UP ---
        if (condition_y_up) {
            // ✅ Requisito atendido: Fazer o LED piscar ao invés de acender continuamente. Lógica de temporização para piscar o LED Vermelho.
            if (absolute_time_diff_us(last_blink_time_vermelho, current_time) / 1000 > BLINK_INTERVAL_MS) {
                led_vermelho_on_state = !led_vermelho_on_state; // Alterna o estado de blink (true/false)
                gpio_put(LED_VERMELHO, !led_vermelho_on_state); // Atualiza o pino (0=ligado, 1=desligado para LED ativo baixo)
                last_blink_time_vermelho = current_time;      // Atualiza o tempo do último toggle
            }
             // Garante que os outros LEDs estejam desligados enquanto o vermelho pisca
            gpio_put(LED_VERDE, 1);
            gpio_put(LED_AZUL, 1);

            // Ativa o buzzer (se já não estiver no modo temporizado)
            if (is_nil_time(buzzer_off_time)) {
                 ativar_buzzer(VOLUME_ALTO, FREQ_CIMA); // Ativa o buzzer
                 buzzer_off_time = make_timeout_time_ms(DURACAO_BUZZER_MS); // Define o tempo para desligar
            }
        }
        // --- Lógica de controle para Y_DOWN ---
        else if (condition_y_down) {
             // ✅ Requisito atendido: Fazer o LED piscar ao invés de acender continuamente. Lógica de temporização para piscar o LED Verde.
             if (absolute_time_diff_us(last_blink_time_verde, current_time) / 1000 > BLINK_INTERVAL_MS) {
                led_verde_on_state = !led_verde_on_state;
                gpio_put(LED_VERDE, !led_verde_on_state);
                last_blink_time_verde = current_time;
            }
             // Garante que os outros LEDs estejam desligados
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_AZUL, 1);

             // Ativa o buzzer
            if (is_nil_time(buzzer_off_time)) {
                 ativar_buzzer(VOLUME_MEDIO, FREQ_BAIXO);
                 buzzer_off_time = make_timeout_time_ms(DURACAO_BUZZER_MS);
            }
        }
        // --- Lógica de controle para X_RIGHT ---
        else if (condition_x_right) {
             // ✅ Requisito atendido: Fazer o LED piscar ao invés de acender continuamente. Lógica de temporização para piscar o LED Azul.
             if (absolute_time_diff_us(last_blink_time_azul, current_time) / 1000 > BLINK_INTERVAL_MS) {
                led_azul_on_state = !led_azul_on_state;
                gpio_put(LED_AZUL, !led_azul_on_state);
                last_blink_time_azul = current_time;
            }
             // Garante que os outros LEDs estejam desligados
            gpio_put(LED_VERMELHO, 1);
            gpio_put(LED_VERDE, 1);

             // Ativa o buzzer
             if (is_nil_time(buzzer_off_time)) {
                 ativar_buzzer(VOLUME_BAIXO, FREQ_DIREITA);
                 buzzer_off_time = make_timeout_time_ms(DURACAO_BUZZER_MS);
             }
        }
        // --- Se o joystick não estiver em nenhuma das posições de acionamento ---
        else {
            // ✅ Requisito atendido: Fazer o LED piscar ao invés de acender continuamente. Desliga os LEDs quando a condição não é mais atendida.
            gpio_put(LED_VERMELHO, 1); // Desliga LED Vermelho
            gpio_put(LED_VERDE, 1);    // Desliga LED Verde
            gpio_put(LED_AZUL, 1);     // Desliga LED Azul
            // Reseta os estados de blink para garantir que comecem 'desligados' na próxima ativação
            led_vermelho_on_state = false;
            led_verde_on_state = false;
            led_azul_on_state = false;
            // O buzzer é desativado pelo temporizador, então não precisamos desativá-lo explicitamente aqui
        }


        // Pequena pausa antes da próxima leitura
        sleep_ms(INTERVALO_MS);
    }
    return 0; // Embora o loop seja infinito, boa prática incluir um return 0
}
