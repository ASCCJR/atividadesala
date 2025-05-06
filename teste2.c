#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

// Definições de pinos e canais ADC
#define JOY_Y_ADC_CHANNEL 1    // Canal ADC 1 está conectado ao GPIO27, que é o eixo Y do joystick
#define JOY_X_ADC_CHANNEL 0    // Canal ADC 0 está conectado ao GPIO26, que é o eixo X do joystick
#define LED_VERMELHO 13        // GPIO13 conectado ao LED Vermelho 
#define LED_VERDE 11           // GPIO11 conectado ao LED Verde 
#define LED_AZUL 12            // GPIO12 conectado ao LED Azul 
#define BUZZER_PIN 21          // GPIO21 conectado ao buzzer 

// Usamos GPIO21 porque ele possui funcionalidade PWM (Pulse Width Modulation)
// necessária para controlar o volume do buzzer, e está conectado através de um
// transistor que atua como uma chave para controlar a corrente que passa pelo buzzer.

// Definições de tempo e limites
#define INTERVALO_MS 50        // Intervalo de tempo em milissegundos entre cada leitura do joystick
#define LIMITE_ACIONAMENTO_Y_UP 3500    // Valor limite de leitura do ADC para considerar o joystick pressionado para cima no eixo Y
#define LIMITE_ACIONAMENTO_Y_DOWN 1000  // Valor limite de leitura do ADC para considerar o joystick pressionado para baixo no eixo Y
#define LIMITE_ACIONAMENTO_X_RIGHT 3500 // Valor limite de leitura do ADC para considerar o joystick pressionado para a direita no eixo X
#define DURACAO_BUZZER_MS 300  // Duração em milissegundos que o buzzer soará após uma detecção de movimento

// Definições de volume do buzzer (controlado pelo duty cycle do PWM, escala de 0 a 255)
#define VOLUME_ALTO 220        // Duty cycle alto para um volume mais alto
#define VOLUME_MEDIO 160       // Duty cycle médio para um volume médio
#define VOLUME_BAIXO 100       // Duty cycle baixo para um volume mais baixo

// Definições de frequência do buzzer (em Hertz)
#define FREQ_CIMA 3000         // Frequência mais alta para um tom agudo (associado ao movimento para cima)
#define FREQ_BAIXO 1500        // Frequência média para um tom médio (associado ao movimento para baixo)
#define FREQ_DIREITA 800       // Frequência mais baixa para um tom grave (associado ao movimento para a direita)

// Variáveis globais para armazenar o número da 'slice' PWM
uint slice_num, channel;

// Função para configurar o PWM no pino conectado ao buzzer.
// Isso permite controlar a frequência e o duty cycle (para o volume) do sinal enviado ao buzzer.
void configurar_pwm_buzzer() {
    // Define a função do GPIO do buzzer para PWM
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    // Obtém o número da 'slice' PWM que o pino do buzzer está usando
    slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    // Obtém o número do 'channel' PWM dentro dessa 'slice' que o pino do buzzer está usando
    channel = pwm_gpio_to_channel(BUZZER_PIN);
    // Inicializa o duty cycle em 0, o que significa que o buzzer começa desligado (sem som)
    pwm_set_chan_level(slice_num, channel, 0);
}

// Função para ativar o buzzer com um determinado volume e frequência.
// O volume é controlado pelo 'duty cycle' do PWM, e a frequência afeta o tom do som.
void ativar_buzzer(uint16_t volume, uint32_t frequencia) {
    // Configura a frequência do PWM
    uint32_t clock = 125000000; // Clock do sistema Pico é de 125MHz
    float divisor = 16.0;       // Define um divisor para alcançar a frequência desejada
    uint32_t wrap = clock / divisor / frequencia; // Calcula o valor de 'wrap' para a frequência alvo

    // Obtém a configuração padrão do PWM
    pwm_config config = pwm_get_default_config();
    // Aplica o divisor de clock calculado à configuração
    pwm_config_set_clkdiv(&config, divisor);
    // Define o valor de 'wrap' (o período do PWM) na configuração
    pwm_config_set_wrap(&config, wrap);
    // Inicializa o PWM com a configuração definida na 'slice' do buzzer e o habilita
    pwm_init(slice_num, &config, true);

    // Configura o volume do buzzer definindo o nível do 'channel' (duty cycle).
    // O valor de 'volume' (0-255) determina a porcentagem do tempo que o sinal PWM fica em nível alto.
    pwm_set_chan_level(slice_num, channel, volume);
}

// Função para desativar o buzzer, configurando o duty cycle para 0.
void desativar_buzzer() {
    pwm_set_chan_level(slice_num, channel, 0);
}

int main() {
    // Inicializa a comunicação serial para poder usar o printf
    stdio_init_all();
    // Pequena pausa para permitir a inicialização da serial
    sleep_ms(9000);
    printf("Sistema Joystick com Volume e Frequencia Variaveis\n");

    // Inicializa os pinos dos LEDs como saídas
    gpio_init(LED_VERMELHO);
    gpio_init(LED_VERDE);
    gpio_init(LED_AZUL);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_set_dir(LED_AZUL, GPIO_OUT);
    // Desliga todos os LEDs inicialmente
    gpio_put(LED_VERMELHO, 1);
    gpio_put(LED_VERDE, 1);
    gpio_put(LED_AZUL, 1);

    // Configura o PWM para controlar o buzzer
    configurar_pwm_buzzer();

    // Inicializa o ADC (Analog-to-Digital Converter)
    adc_init();
    // Inicializa os pinos GPIO conectados aos eixos X e Y do joystick como entradas ADC
    adc_gpio_init(26); // GPIO26 é o pino de entrada para o eixo X (ADC0)
    adc_gpio_init(27); // GPIO27 é o pino de entrada para o eixo Y (ADC1)

    // Variável para armazenar o tempo em que o buzzer deve ser desligado.
    // Inicialmente, está como 'nil_time', indicando que o buzzer não está programado para desligar.
    absolute_time_t buzzer_off_time = nil_time;

    // Loop principal do programa
    while (true) {
        // Leitura dos valores analógicos dos eixos X e Y do joystick
        adc_select_input(JOY_Y_ADC_CHANNEL); // Seleciona o canal ADC para o eixo Y
        uint16_t y = adc_read();           // Lê o valor analógico do eixo Y (0 - 4095)
        adc_select_input(JOY_X_ADC_CHANNEL); // Seleciona o canal ADC para o eixo X
        uint16_t x = adc_read();           // Lê o valor analógico do eixo X (0 - 4095)

        // Verifica se o tempo para desligar o buzzer já passou
        if (!is_nil_time(buzzer_off_time) && time_reached(buzzer_off_time)) {
            desativar_buzzer();            // Desativa o buzzer
            buzzer_off_time = nil_time;    // Reseta o tempo de desligamento
        }

        // Lógica de controle dos LEDs e do buzzer baseada na posição do joystick
        if (y > LIMITE_ACIONAMENTO_Y_UP) { // Se o valor Y for maior que o limite superior (movimento para cima)
            gpio_put(LED_VERMELHO, 0);   // Acende o LED Vermelho
            gpio_put(LED_VERDE, 1);      // Apaga o LED Verde
            gpio_put(LED_AZUL, 1);       // Apaga o LED Azul
            ativar_buzzer(VOLUME_ALTO, FREQ_CIMA); // Ativa o buzzer com volume alto e tom agudo
            buzzer_off_time = make_timeout_time_ms(DURACAO_BUZZER_MS); // Define o tempo para desligar o buzzer
        }
        else if (y < LIMITE_ACIONAMENTO_Y_DOWN) { // Se o valor Y for menor que o limite inferior (movimento para baixo)
            gpio_put(LED_VERDE, 0);      // Acende o LED Verde
            gpio_put(LED_VERMELHO, 1);   // Apaga o LED Vermelho
            gpio_put(LED_AZUL, 1);       // Apaga o LED Azul
            ativar_buzzer(VOLUME_MEDIO, FREQ_BAIXO); // Ativa o buzzer com volume médio e tom médio
            buzzer_off_time = make_timeout_time_ms(DURACAO_BUZZER_MS); // Define o tempo para desligar o buzzer
        }
        else if (x > LIMITE_ACIONAMENTO_X_RIGHT) { // Se o valor X for maior que o limite para a direita
            gpio_put(LED_AZUL, 0);       // Acende o LED Azul
            gpio_put(LED_VERMELHO, 1);   // Apaga o LED Vermelho
            gpio_put(LED_VERDE, 1);      // Apaga o LED Verde
            ativar_buzzer(VOLUME_BAIXO, FREQ_DIREITA); // Ativa o buzzer com volume baixo e tom grave
            buzzer_off_time = make_timeout_time_ms(DURACAO_BUZZER_MS); // Define o tempo para desligar o buzzer
        }
        else { // Se o joystick não estiver em nenhuma das posições de acionamento
            gpio_put(LED_VERMELHO, 1);   // Apaga o LED Vermelho
            gpio_put(LED_VERDE, 1);      // Apaga o LED Verde
            gpio_put(LED_AZUL, 1);       // Apaga o LED Azul
            // O buzzer é desativado pelo temporizador, então não precisamos desativá-lo explicitamente aqui
        }

        // Pequena pausa antes da próxima leitura
        sleep_ms(INTERVALO_MS);
    }
}