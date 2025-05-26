# Sistema de Controle de Acesso 

Este projeto implementa um sistema de controle de capacidade com entrada e saída de pessoas, usando um microcontrolador **Raspberry Pi Pico** e um display **OLED I2C**, com lógica concorrente baseada em **FreeRTOS**.

O sistema conta e exibe o número de pessoas presentes em um ambiente, utilizando semáforos e mutexes para coordenar tarefas e recursos compartilhados, como display e LEDs. Também há suporte a alarme sonoro (buzzer) e lógica de controle visual via LEDs.

---

## 🔧 Funcionalidades


- **Gerenciamento de Vagas**  
  - Semáforo de contagem (`xContadorSemph`) controla até `CAPACIDADE` vagas.  
  - Ao pressionar “Entrada” (BOTÃO_A), tenta-se incrementar o semáforo (+1 vaga ocupada).  
  - Ao pressionar “Saída” (BOTÃO_B), tenta-se decrementar o semáforo (–1 vaga ocupada).  
  - Se o estacionamento estiver **lotado** ou **vazio**, há feedback visual e sonoro.

- **Reset de Contador**  
  - BOTÃO de reset (JOYSTICK_BT) libera um **semáforo binário** (`xBinSemphReset`) que aciona a tarefa de reset.  
  - Ao resetar, buzzer emite dois bipes e o semáforo de contagem é recriado com valor zero.

- **Interface com Usuário**  
  - **Display OLED SSD1306** exibe:  
    - “CONTROLE”  
    - “CAPACIDADE”  
    - Quantidade atual de vagas ocupadas (`n / CAPACIDADE`)  
    - Estado textual: “LOTADO!”, “1 VAGA!”, “DISPONÍVEL” ou “VAZIO”.  
  - **LEDs RGB** mudam de cor conforme o nível de ocupação:  
    - Vermelho = lotado  
    - Amarelo = última vaga  
    - Verde = vagas disponíveis  
    - Azul = vazio  
  - **Buzzer PWM** emite alertas curtos quando o estacionamento atinge lotação máxima ou ao resetar.

- **Sincronização de Tarefas**  
  - **Semáforo de contagem** (`xContadorSemph`) para vagas.  
  - **Semáforo binário** para cada botão (`xBinSemphEntrada`, `xBinSemphSaida`, `xBinSemphReset`).  
  - **Mutex** (`xMutexDisplay`) para proteger o acesso ao display OLED.


---

## 📦 Requisitos

- Raspberry Pi Pico ou Pico W
- Display OLED I2C (SSD1306)
- FreeRTOS (para RP2040)
- SDK do Raspberry Pi Pico
- BitDogLab com botões e LEDs conectados
- Buzzer conectado ao pino PWM

---

## 🧰 Hardware Mapeado

| Componente      | Pino GPIO | Função                          |
|----------------|------------|---------------------------------|
| Botão A         | GPIO 5     | Entrada                         |
| Botão B         | GPIO 6     | Saída                           |
| Botão Joystick  | GPIO 22    | Reset do sistema                |
| LED Vermelho    | GPIO 13    | Indicação de "lotado"           |
| LED Verde       | GPIO 11    | Indicação de capacidade disponível |
| LED Azul        | GPIO 12    | Indicação de vazio              |
| Buzzer (PWM)    | GPIO 21    | Alarme sonoro                   |
| OLED SDA        | GPIO 14    | Comunicação I2C                 |
| OLED SCL        | GPIO 15    | Comunicação I2C                 |

---

## 🧰 Hardware e Bibliotecas

- **Placa**: Raspberry Pi Pico (RP2040)  
- **Display OLED**: SSD1306 via I2C (`lib/ssd1306.h`, `lib/font.h`)  
- **LEDs RGB**: GPIOs 11 (verde), 12 (azul) e 13 (vermelho)  
- **Buzzer**: GPIO 21 (PWM)  
- **Botões**:  
  - JOYSTICK_BT (GPIO 22) → Reset  
  - BOTÃO_A (GPIO 5) → Entrada  
  - BOTÃO_B (GPIO 6) → Saída  
- **RTOS**: FreeRTOS (`FreeRTOS.h`, `semphr.h`)  
- **I2C/PWM/GPIO**: SDK da Raspberry Pi Pico

---

## 🚦 Lógica de Capacidade

- **Capacidade Máxima:** 15 pessoas
- **Contador:** Armazena quantas pessoas estão no local
- **Estados do LED:**
  - 0 pessoas: Azul
  - 1 a 13 pessoas: Verde
  - 14 pessoas: Amarelo
  - 15 pessoas: Vermelho

---

## ▶️ Como Rodar

1. Clone o repositório e configure o ambiente de build do Pico SDK com suporte a FreeRTOS.
2. Conecte os periféricos conforme o mapeamento acima.
3. Compile o projeto usando `cmake` e `make`.
4. Envie o firmware para o Pico via USB (modo BOOTSEL).
5. O sistema já iniciará e mostrará "0/10 - Disponível" no display.

---

## 📸 Exemplo de Saída (Display OLED)

+----------------------------+
| CONTROLE |
| CAPACIDADE |
| 3 / 15 |
| Disponivel |
+----------------------------+

---

## 💡 Possíveis Extensões

- Contador automático com sensores infravermelho.
- Registro de tempo de permanência.
- Envio de dados via Wi-Fi (Pico W).
- Interface web embarcada.

