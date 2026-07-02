#include "stm32f4xx.h"
#include "string.h"

// USER SETTINGS
#define RAIN_ACTIVE_LOW       1U
#define MOTION_ACTIVE_LOW     1U
#define BUTTON_ACTIVE_LOW     1U

// Servo PWM width 
#define SERVO_CLOSED_US       500U
#define SERVO_OPEN_US         1500U

// Doorbell open duration 
#define BUTTON_OPEN_TIME_MS   3000U

// PIN DEFINITIONS 
// Inputs 
#define RAIN_PIN              0U      // PA0 
#define MOTION_PIN            1U      // PA1 
#define BUTTON_PIN            1U      // PB1 

// LCD 
#define LCD_RS_PIN            5U      // PB5 
#define LCD_EN_PIN            6U      // PB6 
#define LCD_D4_PIN            4U      // PA4 
#define LCD_D5_PIN            5U      // PA5 
#define LCD_D6_PIN            6U      // PA6 
#define LCD_D7_PIN            7U      // PA7 
#define LCD_DATA_MASK         ((1U << LCD_D4_PIN) | (1U << LCD_D5_PIN) | (1U << LCD_D6_PIN) | (1U << LCD_D7_PIN))

// Outputs 
#define SERVO_PIN             8U      // PA8 = TIM1_CH1 
#define BUZZER_PIN            10U     // PB10 
#define LED_RAIN_PIN          12U     // PB12 
#define LED_BUTTON_PIN        13U     // PB13 
#define LED_MOTION_PIN        14U     // PB14 
#define LED_IDLE_PIN          15U     // PB15 
#define LED_ALL_MASK          ((1U << LED_RAIN_PIN) | (1U << LED_BUTTON_PIN) | (1U << LED_MOTION_PIN) | (1U << LED_IDLE_PIN))

#define PIN_SET(PORT, PIN)        ((PORT)->BSRR = (1U << (PIN)))
#define PIN_RESET(PORT, PIN)      ((PORT)->BSRR = (1U << ((PIN) + 16U)))

// GLOBAL VARIABLES 
volatile uint32_t g_ms_ticks = 0;
volatile uint8_t g_button_irq_flag = 0;
volatile uint32_t g_last_button_irq_ms = 0;

// STATE MACHINE 
typedef enum
{
    STATE_IDLE = 0,
    STATE_RAIN,
    STATE_BUTTON,
    STATE_MOTION
} DoorState;

// TIME BASE 
void SysTick_Handler(void)
{
    g_ms_ticks++;
}

static uint32_t millis(void)
{
    return g_ms_ticks;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms)
    {
        // wait 
    }
}

// LCD FUNCTIONS 
static void lcd_enable_pulse(void)
{
    PIN_SET(GPIOB, LCD_EN_PIN);
    delay_ms(1);
    PIN_RESET(GPIOB, LCD_EN_PIN);
    delay_ms(1);
}

static void lcd_write4(uint8_t nibble)
{
    uint32_t set_mask = 0U;

    // Clear PA4 to PA7 
    GPIOA->BSRR = (LCD_DATA_MASK << 16U);

    if ((nibble & 0x01U) != 0U) set_mask |= (1U << LCD_D4_PIN);
    if ((nibble & 0x02U) != 0U) set_mask |= (1U << LCD_D5_PIN);
    if ((nibble & 0x04U) != 0U) set_mask |= (1U << LCD_D6_PIN);
    if ((nibble & 0x08U) != 0U) set_mask |= (1U << LCD_D7_PIN);

    GPIOA->BSRR = set_mask;
    lcd_enable_pulse();
}

static void lcd_send(uint8_t value, uint8_t rs)
{
    if (rs != 0U)
        PIN_SET(GPIOB, LCD_RS_PIN);
    else
        PIN_RESET(GPIOB, LCD_RS_PIN);

    lcd_write4((uint8_t)(value >> 4));
    lcd_write4((uint8_t)(value & 0x0F));
    delay_ms(2);
}

static void lcd_cmd(uint8_t cmd)
{
    lcd_send(cmd, 0U);
}

static void lcd_data(uint8_t data)
{
    lcd_send(data, 1U);
}

static void lcd_init(void)
{
    delay_ms(50);

    PIN_RESET(GPIOB, LCD_RS_PIN);
    PIN_RESET(GPIOB, LCD_EN_PIN);

    // LCD 4-bit initialization sequence 
    lcd_write4(0x03U);
    delay_ms(5);
    lcd_write4(0x03U);
    delay_ms(5);
    lcd_write4(0x03U);
    delay_ms(5);
    lcd_write4(0x02U);
    delay_ms(5);

    lcd_cmd(0x28U);   // 4-bit, 2 lines, 5x8 font 
    lcd_cmd(0x0CU);   // Display ON, cursor OFF 
    lcd_cmd(0x06U);   // Entry mode 
    lcd_cmd(0x01U);   // Clear display 
    delay_ms(5);
}

static void lcd_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t address;

    if (row == 0U)
        address = (uint8_t)(0x00U + col);
    else
        address = (uint8_t)(0x40U + col);

    lcd_cmd((uint8_t)(0x80U | address));
}

static void lcd_print_16(const char *text)
{
    uint8_t i;

    for (i = 0U; i < strlen(text); i++)
    {
        if (text[i] != '\0')
            lcd_data((uint8_t)text[i]);
        else
            lcd_data(' ');
    }
}

static void lcd_show(const char *line1, const char *line2)
{
    lcd_set_cursor(0U, 0U);
    lcd_print_16(line1);
    lcd_set_cursor(1U, 0U);
    lcd_print_16(line2);
}

// OUTPUT FUNCTIONS
static void buzzer_on(void)
{
	uint8_t i;
    for(i = 0; i < 1; i++)
		{
			PIN_SET(GPIOB, BUZZER_PIN);
			delay_ms(200);
			PIN_RESET(GPIOB, BUZZER_PIN);
			delay_ms(150);
			PIN_SET(GPIOB, BUZZER_PIN);
			delay_ms(200);
			PIN_RESET(GPIOB, BUZZER_PIN);
			delay_ms(500);
		}
}

static void buzzer_off(void)
{
    PIN_RESET(GPIOB, BUZZER_PIN);
}

static void led_all_off(void)
{
    GPIOB->BSRR = (LED_ALL_MASK << 16U);
}

static void led_show(uint32_t led_mask)
{
    led_all_off();
    GPIOB->BSRR = led_mask;
}

static void servo_set_us(uint16_t pulse_us)
{
    TIM1->CCR1 = pulse_us;
}

static void servo_open(void)
{
    servo_set_us(SERVO_OPEN_US);
}

static void servo_close(void)
{
    servo_set_us(SERVO_CLOSED_US);
}

// INPUT FUNCTIONS
static uint8_t read_active(GPIO_TypeDef *port, uint8_t pin, uint8_t active_low)
{
    uint8_t pin_is_high;

    pin_is_high = ((port->IDR & (1U << pin)) != 0U) ? 1U : 0U;

    if (active_low != 0U)
        return (pin_is_high == 0U) ? 1U : 0U;
    else
        return (pin_is_high != 0U) ? 1U : 0U;
}

static uint8_t is_rain_detected(void)
{
    return read_active(GPIOA, RAIN_PIN, RAIN_ACTIVE_LOW);
}

static uint8_t is_motion_detected(void)
{
    return read_active(GPIOA, MOTION_PIN, MOTION_ACTIVE_LOW);
}

static uint8_t is_button_pressed(void)
{
    return read_active(GPIOB, BUTTON_PIN, BUTTON_ACTIVE_LOW);
}

// BUTTON INTERRUPT 
void EXTI1_IRQHandler(void)
{
    if ((EXTI->PR & EXTI_PR_PR1) != 0U)
    {
        EXTI->PR = EXTI_PR_PR1;

        // ignore repeated button interrupts within 200 ms 
        if ((g_ms_ticks - g_last_button_irq_ms) > 200U)
        {
            g_button_irq_flag = 1U;
            g_last_button_irq_ms = g_ms_ticks;
        }
    }
}

// INITIALIZATION 
static void gpio_init(void)
{
    // Enable GPIOA and GPIOB clock 
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;

    /* GPIOA:
     * PA0 input  = rain sensor
     * PA1 input  = avoid sensor
     * PA4-PA7 output = LCD data
     * PA8 alternate function = TIM1_CH1 servo
     */
    GPIOA->MODER &= ~((3U << (RAIN_PIN * 2U)) |
                      (3U << (MOTION_PIN * 2U)) |
                      (3U << (LCD_D4_PIN * 2U)) |
                      (3U << (LCD_D5_PIN * 2U)) |
                      (3U << (LCD_D6_PIN * 2U)) |
                      (3U << (LCD_D7_PIN * 2U)) |
                      (3U << (SERVO_PIN * 2U)));

    GPIOA->MODER |=  ((1U << (LCD_D4_PIN * 2U)) |
                      (1U << (LCD_D5_PIN * 2U)) |
                      (1U << (LCD_D6_PIN * 2U)) |
                      (1U << (LCD_D7_PIN * 2U)) |
                      (2U << (SERVO_PIN * 2U)));

    // Pull-up input for PA0 and PA1
    GPIOA->PUPDR &= ~((3U << (RAIN_PIN * 2U)) |
                      (3U << (MOTION_PIN * 2U)));
    GPIOA->PUPDR |=  ((1U << (RAIN_PIN * 2U)) |
                      (1U << (MOTION_PIN * 2U)));

    // PA8 alternate function AF1 = TIM1_CH1 
    GPIOA->AFR[1] &= ~(0xFU << ((SERVO_PIN - 8U) * 4U));
    GPIOA->AFR[1] |=  (0x1U << ((SERVO_PIN - 8U) * 4U));

    /* GPIOB:
     * PB1 input = button
     * PB5 output = LCD RS
     * PB6 output = LCD EN
     * PB10 output = buzzer
     * PB12-PB15 output = LEDs
     */
    GPIOB->MODER &= ~((3U << (BUTTON_PIN * 2U)) |
                      (3U << (LCD_RS_PIN * 2U)) |
                      (3U << (LCD_EN_PIN * 2U)) |
                      (3U << (BUZZER_PIN * 2U)) |
                      (3U << (LED_RAIN_PIN * 2U)) |
                      (3U << (LED_BUTTON_PIN * 2U)) |
                      (3U << (LED_MOTION_PIN * 2U)) |
                      (3U << (LED_IDLE_PIN * 2U)));

    GPIOB->MODER |=  ((1U << (LCD_RS_PIN * 2U)) |
                      (1U << (LCD_EN_PIN * 2U)) |
                      (1U << (BUZZER_PIN * 2U)) |
                      (1U << (LED_RAIN_PIN * 2U)) |
                      (1U << (LED_BUTTON_PIN * 2U)) |
                      (1U << (LED_MOTION_PIN * 2U)) |
                      (1U << (LED_IDLE_PIN * 2U)));

    // PB1 pull-up button input 
    GPIOB->PUPDR &= ~(3U << (BUTTON_PIN * 2U));
    GPIOB->PUPDR |=  (1U << (BUTTON_PIN * 2U));

    buzzer_off();
    led_all_off();
    PIN_RESET(GPIOB, LCD_RS_PIN);
    PIN_RESET(GPIOB, LCD_EN_PIN);
}

static void exti_button_init(void)
{
    // Enable SYSCFG clock
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;

    // EXTI1 source = PB1
    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI1;
    SYSCFG->EXTICR[0] |=  SYSCFG_EXTICR1_EXTI1_PB;

    // Falling edge trigger for active-low button
    EXTI->IMR  |= EXTI_IMR_MR1;
    EXTI->FTSR |= EXTI_FTSR_TR1;
    EXTI->RTSR &= ~EXTI_RTSR_TR1;

    // Clear pending flag 
    EXTI->PR = EXTI_PR_PR1;

    NVIC_SetPriority(EXTI1_IRQn, 2U);
    NVIC_EnableIRQ(EXTI1_IRQn);
}

static void servo_pwm_init(void)
{
    uint32_t prescaler;

    // Enable TIM1 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    (void)RCC->APB2ENR;

    // Timer tick = 1 us. PWM period = 20 ms.
    prescaler = (SystemCoreClock / 1000000U) - 1U;

    TIM1->PSC = prescaler;
    TIM1->ARR = 19999U;
    TIM1->CCR1 = SERVO_CLOSED_US;

    // PWM mode 1 on CH1
    TIM1->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM1->CCMR1 |=  (6U << TIM_CCMR1_OC1M_Pos);
    TIM1->CCMR1 |=  TIM_CCMR1_OC1PE;
    TIM1->CCER |= TIM_CCER_CC1E;
    TIM1->CR1  |= TIM_CR1_ARPE;

    // Main output enable for advanced timer TIM1 
    TIM1->BDTR |= TIM_BDTR_MOE;

    TIM1->EGR = TIM_EGR_UG;
    TIM1->CR1 |= TIM_CR1_CEN;
}

// SWITCH-CASE STATE FUNCTIONS 
static DoorState get_next_state(uint32_t button_open_until, uint32_t motion_open_until)
{
    // Priority: rain > button > avoid sensor > idle 
    if (is_rain_detected() != 0U)
    {
				return STATE_RAIN;
    }
    else if ((int32_t)(button_open_until - millis()) > 0)
    {
        return STATE_BUTTON;
    }
    else if ((int32_t)(motion_open_until - millis()) > 0)
    {
				return STATE_MOTION;
    }
    else
    {
        return STATE_IDLE;
    }
}

static void apply_state(DoorState state)
{
    switch (state)
    {
        case STATE_RAIN:
            servo_close();
            led_show(1U << LED_RAIN_PIN);
						lcd_cmd(0x01U);
            lcd_show("RAIN DETECTED", "DOOR CLOSED");
						buzzer_on();
            break;

        case STATE_BUTTON:
            servo_open();
            led_show(1U << LED_BUTTON_PIN);
						lcd_cmd(0x01U);
            lcd_show("DOORBELL", "DOOR OPEN");
						buzzer_on();
            break;

        case STATE_MOTION:
            servo_open();
            buzzer_off();
            led_show(1U << LED_MOTION_PIN);
						lcd_cmd(0x01U);
            lcd_show("OBJECT DETECT", "DOOR OPEN");
            break;

        case STATE_IDLE:
        default:
            servo_close();
            buzzer_off();
            led_show(1U << LED_IDLE_PIN);
						lcd_cmd(0x01U);
            lcd_show("SMART DOOR", "DOOR CLOSED");
            break;
    }
}

// MAIN PROGRAM
int main(void)
{
    DoorState current_state = STATE_IDLE;
    DoorState next_state = STATE_IDLE;
    uint32_t button_open_until = 0U;
		uint32_t motion_open_until = 0U;

    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000U);

    gpio_init();
    exti_button_init();
    servo_pwm_init();
    lcd_init();

    lcd_show("SMART SECURITY", "DOOR SYSTEM");
    led_show(1U << LED_IDLE_PIN);
    delay_ms(1000);

    apply_state(STATE_IDLE);

    while (1)
    {
        if ((g_button_irq_flag != 0U) || (is_button_pressed() != 0U))
        {
            g_button_irq_flag = 0U;
            button_open_until = millis() + BUTTON_OPEN_TIME_MS;
        }
				
				if (is_motion_detected() != 0U)
        {
            motion_open_until = millis() + 2000;
        }

        next_state = get_next_state(button_open_until, motion_open_until);

        if (next_state != current_state)
        {
            current_state = next_state;
            apply_state(current_state);
        }
				
				if (next_state == STATE_RAIN)
				{
						motion_open_until = 0;
				}

        delay_ms(50);
    }
}
