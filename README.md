# 🚦 Semáforo Inteligente com FreeRTOS - BitDogLab + RP2040

Este projeto foi desenvolvido como atividade prática para consolidar os conhecimentos adquiridos sobre tarefas no sistema operacional em tempo real **FreeRTOS**, utilizando a placa **BitDogLab** com o microcontrolador **RP2040**.

## 🎯 Objetivo

Criar um sistema de semáforo inteligente com dois modos de operação distintos, controlado exclusivamente por tarefas do FreeRTOS, **sem utilizar filas, semáforos ou mutexes**. O projeto também contempla **acessibilidade para pessoas com deficiência visual**, utilizando sinais sonoros distintos para cada estado do semáforo.

---

## ⚙️ Funcionalidades

### 🔄 Modos de Operação

- **Modo Normal**:
  - Verde: 15 segundos
  - Amarelo: 5 segundos
  - Vermelho: 15 segundos
  - Ciclo contínuo
  - Buzzer com sons específicos para cada cor

- **Modo Noturno**:
  - Luz amarela piscando a cada 2 segundos
  - Buzzer com beep a cada 2 segundos
  - Ativado ao pressionar o **Botão A**

### ♿ Acessibilidade Sonora

- Verde: 1 beep curto por segundo ("Pode atravessar")
- Amarelo: beeps rápidos intermitentes ("Atenção")
- Vermelho: tom contínuo curto de 500ms ("Pare")
- Noturno: beep lento a cada 2s

---

## 🧱 Componentes Utilizados

| Componente       | Função                                    |
|------------------|-------------------------------------------|
| Matriz WS2812    | Representação visual do semáforo          |
| LED RGB          | Alerta visual da troca de estado          |
| Display OLED     | Exibição do modo atual e temporizador     |
| Buzzer           | Sinalização sonora para acessibilidade    |
| Botão A          | Alterna entre os modos                    |
| Botão B          | Ativa o modo BOOTSEL                      |

---

## 🧵 Estrutura de Tarefas (FreeRTOS)

| Tarefa              | Responsabilidade                                      |
|---------------------|-------------------------------------------------------|
| `vSemaforoControleTask` | Controla o ciclo do semáforo e alterna os estados     |
| `vMatrizLedsTask`   | Atualiza a matriz de LEDs conforme o estado atual     |
| `vDisplayTask`      | Atualiza o display OLED com o modo e o tempo restante |
| `vBuzzerTask`       | Controla os sons do buzzer conforme o modo/estado     |
| `vLedRgbTask`       | Pisca o LED RGB antes da troca de estado              |
| `vBotoesTask`       | Verifica o botão A e alterna entre os modos           |

---
