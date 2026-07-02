#include "stm32f1xx.h"

// Sensores
#define SENSOR1_PIN  0   // PA0
#define SENSOR2_PIN  2   // PA2
#define SENSOR3_PIN  4   // PA4
#define SENSOR4_PIN  1   // PA1

// LEDs Vagas 1 e 2
#define V1_VERM_PIN  0   // PB0
#define V1_VERD_PIN  1   // PB1
#define V2_VERM_PIN  5   // PB5
#define V2_VERD_PIN  6   // PB6

// LEDs Vagas 3 e 4
#define V3_VERM_PIN  12  // PB12 (Vaga 3 Vermelho)
#define V3_VERD_PIN  13  // PB13 (Vaga 3 Verde)
#define V4_VERM_PIN  14  // PB14 (Vaga 4 Vermelho)
#define V4_VERD_PIN  15  // PB15 (Vaga 4 Verde)

#define USART1_ENVIA_CHAR(c)  do { while (!(USART1->SR & USART_SR_TXE)); USART1->DR = (c); } while(0)

volatile uint32_t ticks = 0;
volatile uint8_t verificar_sensor = 0;

// Inicializamos com 99 para forçar o sistema a enviar o status assim que ligar
uint8_t estado_ant_vaga1 = 99;
uint8_t estado_ant_vaga2 = 99;
uint8_t estado_ant_vaga3 = 99;
uint8_t estado_ant_vaga4 = 99;

void HARDWARE_Init(void);
void USART1_Envia_String(char *str);
void Envia_Relatorio_Estacionamento(uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4, uint8_t livres);

int main(void) {
    HARDWARE_Init();

    for (volatile int i = 0; i < 500000; i++);

    USART1_Envia_String("--- MONITOR DE ESTACIONAMENTO 4 VAGAS ---\r\n\r\n");

    while (1) {
        if (verificar_sensor) {
            verificar_sensor = 0;

            // Leitura individual (0 = Ocupado, 1 = Livre)
            uint8_t vaga1_ocupada = !(GPIOA->IDR & (1 << SENSOR1_PIN));
            uint8_t vaga2_ocupada = !(GPIOA->IDR & (1 << SENSOR2_PIN));
            uint8_t vaga3_ocupada = !(GPIOA->IDR & (1 << SENSOR3_PIN));
            uint8_t vaga4_ocupada = !(GPIOA->IDR & (1 << SENSOR4_PIN));

            uint8_t vagas_livres = 4 - (vaga1_ocupada + vaga2_ocupada + vaga3_ocupada + vaga4_ocupada);

            // --- CONTROLE: VAGA 1 ---
            if (vaga1_ocupada) { GPIOB->BSRR = (1 << V1_VERM_PIN); GPIOB->BRR  = (1 << V1_VERD_PIN); }
            else               { GPIOB->BSRR = (1 << V1_VERD_PIN); GPIOB->BRR  = (1 << V1_VERM_PIN); }

            // --- CONTROLE: VAGA 2 ---
            if (vaga2_ocupada) { GPIOB->BSRR = (1 << V2_VERM_PIN); GPIOB->BRR  = (1 << V2_VERD_PIN); }
            else               { GPIOB->BSRR = (1 << V2_VERD_PIN); GPIOB->BRR  = (1 << V2_VERM_PIN); }

            // --- CONTROLE: VAGA 3 ---
            if (vaga3_ocupada) { GPIOB->BSRR = (1 << V3_VERM_PIN); GPIOB->BRR  = (1 << V3_VERD_PIN); }
            else               { GPIOB->BSRR = (1 << V3_VERD_PIN); GPIOB->BRR  = (1 << V3_VERM_PIN); }

            // --- CONTROLE: VAGA 4 ---
            if (vaga4_ocupada) { GPIOB->BSRR = (1 << V4_VERM_PIN); GPIOB->BRR  = (1 << V4_VERD_PIN); }
            else               { GPIOB->BSRR = (1 << V4_VERD_PIN); GPIOB->BRR  = (1 << V4_VERM_PIN); }

            // Transmissão Serial apenas em caso de mudança de estado
            if (vaga1_ocupada != estado_ant_vaga1 || vaga2_ocupada != estado_ant_vaga2 ||
                vaga3_ocupada != estado_ant_vaga3 || vaga4_ocupada != estado_ant_vaga4) {

                Envia_Relatorio_Estacionamento(vaga1_ocupada, vaga2_ocupada, vaga3_ocupada, vaga4_ocupada, vagas_livres);

                estado_ant_vaga1 = vaga1_ocupada;
                estado_ant_vaga2 = vaga2_ocupada;
                estado_ant_vaga3 = vaga3_ocupada;
                estado_ant_vaga4 = vaga4_ocupada;
            }
        }
        __WFI();
    }
}

void HARDWARE_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN;

    GPIOB->CRL &= ~0x0FF000FF;
    GPIOB->CRL |=  0x02200022;

    GPIOB->CRH &= ~0xFFFF0000;
    GPIOB->CRH |=  0x22220000;

    GPIOA->CRL &= ~0x000F0FFF;
    GPIOA->CRL |=  0x00040444;

    GPIOA->CRH &= ~0x00000FFF;
    GPIOA->CRH |=  0x000004B0;

    // Baud Rate para 9600 bps
    USART1->BRR = 0x341;
    USART1->CR1 |= USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    // Apaga todas as saídas no boot
    GPIOB->BRR = (1 << V1_VERM_PIN) | (1 << V1_VERD_PIN) |
                 (1 << V2_VERM_PIN) | (1 << V2_VERD_PIN) |
                 (1 << V3_VERM_PIN) | (1 << V3_VERD_PIN) |
                 (1 << V4_VERM_PIN) | (1 << V4_VERD_PIN);

    // Configuração do SysTick Timer para interrupção de 1ms
    SysTick->LOAD = 8000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

void USART1_Envia_String(char *str) {
    while (*str) {
        USART1_ENVIA_CHAR(*str++);
    }
}

void Envia_Relatorio_Estacionamento(uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4, uint8_t livres) {
    USART1_Envia_String("==================================\r\n");

    USART1_Envia_String("Vaga 1: ");
    if (v1) USART1_Envia_String("[ OCUPADA ]\r\n");
    else    USART1_Envia_String("[ LIVRE ]\r\n");

    USART1_Envia_String("Vaga 2: ");
    if (v2) USART1_Envia_String("[ OCUPADA ]\r\n");
    else    USART1_Envia_String("[ LIVRE ]\r\n");

    USART1_Envia_String("Vaga 3: ");
    if (v3) USART1_Envia_String("[ OCUPADA ]\r\n");
    else    USART1_Envia_String("[ LIVRE ]\r\n");

    USART1_Envia_String("Vaga 4: ");
    if (v4) USART1_Envia_String("[ OCUPADA ]\r\n");
    else    USART1_Envia_String("[ LIVRE ]\r\n");

    USART1_Envia_String("Vagas Disponiveis: ");
    USART1_ENVIA_CHAR(livres + '0');
    USART1_Envia_String("\r\n==================================\r\n\r\n");
}

void SysTick_Handler(void) {
    ticks++;
    if (ticks >= 50) {
        ticks = 0;
        verificar_sensor = 1;
    }
}
