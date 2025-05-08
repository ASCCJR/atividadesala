# Controle de Joystick com Feedback Visual e Sonoro no Raspberry Pi Pico

Projeto desenvolvido durante a Residência em Sistemas Embarcados para demonstração de:
- Leitura analógica de joystick com ADC
- Controle PWM de buzzer com volume e frequência variáveis
- Feedback visual através de LEDs RGB
- Temporização sem bloqueio para eventos sonoros

## 🎮 Funcionalidades Principais
- **Leitura Analógica Precisa**
  - Amostragem de 12 bits nos eixos X/Y
  - Filtro software por limiares de acionamento
- **Sistema de Feedback Multissensorial**
  - **Visual**: LED vermelho (cima), verde (baixo), azul (direita)
  - **Auditivo**: Tons com 3 características distintas:
    - Cima: 3KHz (agudo) + volume alto
    - Baixo: 1.5KHz (médio) + volume médio
    - Direita: 800Hz (grave) + volume baixo
- **Controle PWM Avançado**
  - Divisor de clock configurável
  - Wrap value dinâmico para ajuste de frequência
  - Duty cycle variável para controle de volume

## 🔌 Diagrama de Conexões
| Pino Pico | Componente       | Detalhe                  |
|-----------|------------------|--------------------------|
| GPIO26    | Eixo X do Joystick | Canal ADC0              |
| GPIO27    | Eixo Y do Joystick | Canal ADC1              |
| GPIO13    | LED Vermelho      | Resistor 220Ω           |
| GPIO11    | LED Verde         | Resistor 220Ω           |
| GPIO12    | LED Azul          | Resistor 150Ω           |
| GPIO21    | Buzzer            | Via transistor BC337    |

## ⚙️ Configuração do Sistema
1. **Clock do Sistema**: 125MHz (padrão Pico)
2. **Parâmetros PWM**:
   - Divisor de clock: 16
   - Frequências geradas: 800Hz a 3KHz
   - Resolução efetiva: 8 bits (duty cycle 0-255)
3. **Parâmetros ADC**:
   - Taxa de amostragem: ~20Hz (50ms intervalo)
   - Limiares de acionamento:
     - Cima: >3500 (12 bits)
     - Baixo: <1000
     - Direita: >3500

## 🧠 Estrutura do Código
### Componentes Principais
1. **Configuração PWM** (`configurar_pwm_buzzer`)
   - Inicialização do hardware PWM
   - Mapeamento do pino ao subsistema PWM

2. **Controle de Áudio** (`ativar_buzzer/desativar_buzzer`)
   - Cálculo dinâmico de wrap value para frequência
   - Ajuste de duty cycle para volume
   - Temporização precisa com `make_timeout_time_ms`

3. **Lógica de Controle Principal**
   - Polling não bloqueante das entradas analógicas
   - Máquina de estados implícita para direções
   - Gerenciamento de recursos compartilhados (PWM)

## ▶️ Modo de Uso
1. Carregue o firmware no Pico
2. Conecte-se via serial (115200 baud) para logs:
   ```bash
   Sistema Joystick com Volume e Frequencia Variaveis
   ```
3. Opere o joystick nas 3 direções para testar:
   - Cima: LED vermelho + tom agudo
   - Baixo: LED verde + tom médio
   - Direita: LED azul + tom grave

## ⚡ Otimizações Implementadas
1. **Reuso de Canal PWM**: Mesma slice PWM para múltiplas configurações
2. **Controle de Volume por Duty Cycle**: Mais eficiente que modulação por amplitude
3. **Leitura Alternada de Canais ADC**: Minimiza cross-talk
4. **Gestão de Energia**: LEDs desligados em repouso

## 📌 Notas Técnicas
- **Buzzer**: Requer transistor devido à alta corrente (≈20mA)
- **LEDs**: Cátodo comum com resistor calculado para ~10mA
- **Joystick**: Valores calibrados para modelo específico (ajustar limites conforme necessário)
  
