# Controle de Joystick com Feedback Avançado no Raspberry Pi Pico

Projeto desenvolvido durante a Residência em Sistemas Embarcados, aprimorado para demonstração de:
- Leitura analógica suavizada de joystick com ADC
- Controle PWM de buzzer com volume e frequência variáveis
- Feedback visual através de LEDs RGB (agora piscando!)
- Temporização sem bloqueio para eventos sonoros e visuais
- Exibição visual da leitura do ADC no terminal serial

## ✨ Requisitos Solicitados
Este projeto foi desenvolvido atendendo aos seguintes requisitos específicos:
1.  Ajustar o limite de acionamento.
2.  Fazer o LED piscar ao invés de acender continuamente.
3.  Usar outro eixo do joystick.
4.  Exibir uma barra gráfica proporcional ao valor do ADC no terminal.
5.  Implementar uma média móvel de 10 leituras.

## ✅ Como os Requisitos Foram Atendidos

O código atual incorpora as seguintes funcionalidades para cumprir os requisitos:

1.  **Ajustar o limite de acionamento:** Os limites são definidos por constantes (`#define`) no início do código (`LIMITE_ACIONAMENTO_Y_UP`, `LIMITE_ACIONAMENTO_Y_DOWN`, `LIMITE_ACIONAMENTO_X_RIGHT`), sendo facilmente ajustáveis.
2.  **Fazer o LED piscar ao invés de acender continuamente:** Em vez de apenas ligar/desligar o LED, a lógica de controle agora verifica o tempo decorrido e alterna o estado do LED (ligado/desligado) em um intervalo fixo (`BLINK_INTERVAL_MS`) enquanto a condição de acionamento correspondente for satisfeita.
3.  **Usar outro eixo do joystick:** O código original já utilizava um eixo (Y). A versão atual continua usando o eixo Y e também incorpora a leitura e lógica de acionamento baseada no eixo X do joystick.
4.  **Exibir uma barra gráfica proporcional ao valor do ADC no terminal:** Uma função (`print_bar_graph`) foi adicionada para escalar o valor médio lido do ADC para uma largura definida e imprimir uma representação visual em caracteres ('#') no terminal serial, juntamente com o valor numérico.
5.  **Implementar uma média móvel de 10 leituras:** Foram adicionados buffers (arrays `y_readings`, `x_readings`) e variáveis para armazenar as últimas 10 leituras de cada eixo. Antes de aplicar a lógica de controle, a média aritmética dessas 10 leituras é calculada e utilizada como valor suavizado.

## 🎮 Funcionalidades Principais Aprimoradas
-   **Leitura Analógica Suavizada e Visualizada**
    -   Amostragem de 12 bits nos eixos X/Y.
    -   **Aplicação de Média Móvel de 10 leituras** para suavizar variações.
    -   Filtro software por limiares de acionamento, agora aplicados ao valor médio.
    -   **Exibição de Barra Gráfica** no terminal serial mostrando a leitura média proporcionalmente.
-   **Sistema de Feedback Multissensorial**
    -   **Visual**: LED vermelho (cima), verde (baixo), azul (direita), agora **piscando** quando acionados.
    -   **Auditivo**: Tons com 3 características distintas:
        -   Cima: 3KHz (agudo) + volume alto
        -   Baixo: 1.5KHz (médio) + volume médio
        -   Direita: 800Hz (grave) + volume baixo
-   **Controle PWM Avançado**
    -   Divisor de clock configurável.
    -   Wrap value dinâmico para ajuste de frequência.
    -   Duty cycle variável para controle de volume.
-   **Temporização Não Bloqueante**
    -   Uso de `absolute_time_t` para gerenciar o tempo de desligamento do buzzer.
    -   Uso de `absolute_time_t` para gerenciar o intervalo de piscar dos LEDs.

## 🔌 Diagrama de Conexões
| Pino Pico | Componente        | Detalhe                  |
|-----------|-------------------|--------------------------|
| GPIO26    | Eixo X do Joystick | Canal ADC0               |
| GPIO27    | Eixo Y do Joystick | Canal ADC1               |
| GPIO13    | LED Vermelho      | Resistor 220Ω            |
| GPIO11    | LED Verde         | Resistor 220Ω            |
| GPIO12    | LED Azul          | Resistor 150Ω            |
| GPIO21    | Buzzer            | Via transistor BC337     |

## ⚙️ Configuração do Sistema
1.  **Clock do Sistema**: 125MHz (padrão Pico)
2.  **Parâmetros PWM**:
    -   Divisor de clock: 16
    -   Frequências geradas: 800Hz a 3KHz
    -   Resolução efetiva: ~8 bits (duty cycle escalado para o wrap)
    -   Duração do tom do Buzzer: 300ms
3.  **Parâmetros ADC e Processamento**:
    -   Taxa de amostragem: ~20Hz (50ms intervalo principal do loop)
    -   **Tamanho da Média Móvel:** 10 leituras
    -   Limiares de acionamento:
        -   Cima: >3500 (aplicado à média de 12 bits)
        -   Baixo: <1000 (aplicado à média de 12 bits)
        -   Direita: >3500 (aplicado à média de 12 bits)
4.  **Parâmetros de Feedback Visual:**
    -   Intervalo de Piscar dos LEDs: 200ms
    -   Largura da Barra Gráfica no Terminal: 50 caracteres

## 🧠 Estrutura do Código
### Componentes Principais
1.  **Configuração PWM** (`configurar_pwm_buzzer`)
    -   Inicialização do hardware PWM.
    -   Mapeamento do pino do buzzer ao subsistema PWM.
2.  **Controle de Áudio** (`ativar_buzzer/desativar_buzzer`)
    -   Cálculo dinâmico de wrap value para frequência.
    -   Ajuste de duty cycle para volume.
    -   Gerenciamento de desligamento temporizado do buzzer.
3.  **Processamento do ADC** (`update_moving_average`, `get_average`)
    -   Funções para adicionar novas leituras a buffers circulares.
    -   Cálculo eficiente da média das leituras no buffer.
4.  **Exibição da Barra Gráfica** (`print_bar_graph`)
    -   Função para formatar e imprimir uma barra proporcional no terminal.
5.  **Lógica de Controle Principal** (`main` loop)
    -   Leitura contínua (RAW) dos eixos do joystick.
    -   Alimentação das leituras RAW para a média móvel.
    -   Uso dos valores médios para aplicar a lógica de acionamento e os limiares.
    -   Máquina de estados implícita para direções (Cima, Baixo, Direita).
    -   **Gerenciamento da temporização e estado para fazer os LEDs piscarem**.
    -   Ativação do buzzer com parâmetros específicos da direção e definição do timer de desligamento.
    -   **Chamada para imprimir as barras gráficas** dos valores médios.
    -   Gerenciamento do recurso compartilhado (PWM do buzzer).
    -   Pausa para definir a taxa de amostragem/atualização.

## ▶️ Modo de Uso
1.  Carregue o firmware no Pico.
2.  Conecte-se via serial (115200 baud) para visualizar os logs e a barra gráfica:
    ```bash
    Sistema Joystick com Volume e Frequencia Variaveis
    ```
3.  Opere o joystick nas 3 direções para testar:
    -   **Terminal:** Observe a barra gráfica se movimentando e o valor numérico mudando para ambos os eixos.
    -   **Cima:** LED vermelho **piscará** + tocará tom agudo.
    -   **Baixo:** LED verde **piscará** + tocará tom médio.
    -   **Direita:** LED azul **piscará** + tocará tom grave.
    -   No centro: Nenhum LED pisca, buzzer desativado (após o tempo de duração).

## ⚡ Otimizações Implementadas
1.  **Reuso de Canal PWM**: Mesma slice/canal PWM para múltiplas configurações de frequência/volume do buzzer.
2.  **Controle de Volume por Duty Cycle**: Método eficiente para controlar o volume via PWM.
3.  **Leitura Alternada de Canais ADC**: Minimiza potencial cross-talk (embora o impacto seja pequeno em muitos casos).
4.  **Gestão de Energia**: LEDs desligados em repouso (fora das zonas de acionamento).
5.  **Temporização Não Bloqueante**: O loop principal não fica preso esperando o buzzer terminar ou o LED piscar.

## Propósito

Este projeto foi desenvolvido com fins estritamente educacionais e aprendizdo durante a residência em sistemas embarcados pelo embarcatech
