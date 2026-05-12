#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "xparameters.h"
#include "xil_printf.h"
#include "xsysmon.h"
#include "xuartlite.h"
#include "xuartlite_l.h"
#include "xgpio.h"
#include "sleep.h"

// ── XADC Base Address ────────────────────────────────────────────────────────
#ifdef XPAR_XADC_WIZ_0_BASEADDR
    #define MY_XADC_BASE  XPAR_XADC_WIZ_0_BASEADDR
#elif defined(XPAR_AXI_XADC_0_BASEADDR)
    #define MY_XADC_BASE  XPAR_AXI_XADC_0_BASEADDR
#else
    #error "No XADC base address found — check your hardware export"
#endif

// ── UART Base Address for ESP32 ──────────────────────────────────────────────
#ifdef XPAR_AXI_UARTLITE_0_BASEADDR
    #define MY_WIFI_BASE  XPAR_AXI_UARTLITE_0_BASEADDR
#else
    #define MY_WIFI_BASE  XPAR_UARTLITE_0_BASEADDR
#endif

// ── GPIO Base Address for Buzzer ─────────────────────────────────────────────
#ifdef XPAR_AXI_GPIO_0_BASEADDR
    #define MY_GPIO_BASE  XPAR_AXI_GPIO_0_BASEADDR
#else
    #error "No AXI GPIO base address found — add GPIO IP to your block design"
#endif

// ── Phone IP and port ────────────────────────────────────────────────────────
#define PHONE_IP    "10.0.248.251"
#define PHONE_PORT  8080

// ── Temperature alert threshold ──────────────────────────────────────────────
#define TEMP_CRITICAL_C  38.0f

// ── XADC channel assignments ─────────────────────────────────────────────────
// Vaux2  (XSM_CH_AUX_MIN + 2)  → LM35 temperature sensor
// Vaux10 (XSM_CH_AUX_MIN + 10) → LDR light sensor
#define TEMP_CH  (XSM_CH_AUX_MIN + 2)
#define LDR_CH   (XSM_CH_AUX_MIN + 10)

// ── Driver instances ──────────────────────────────────────────────────────────
XSysMon   xadc_inst;
XUartLite esp32_uart;
XGpio     buzzer_gpio;

// ── Send AT command and print ESP32 response ──────────────────────────────────
void send_at_command(char *cmd, int wait_ms) {
    xil_printf(">> %s\r\n<< ", cmd);
    while (*cmd != '\0') {
        XUartLite_SendByte(MY_WIFI_BASE, (u8)*cmd);
        cmd++;
    }
    XUartLite_SendByte(MY_WIFI_BASE, (u8)'\r');
    XUartLite_SendByte(MY_WIFI_BASE, (u8)'\n');

    int loops = wait_ms / 100;
    for (int i = 0; i < loops; i++) {
        while (!XUartLite_IsReceiveEmpty(MY_WIFI_BASE)) {
            u8 b = XUartLite_RecvByte(MY_WIFI_BASE);
            xil_printf("%c", b);
        }
        usleep(100000);
    }
    xil_printf("\r\n");
}

// ── Read one XADC aux channel manually ────────────────────────────────────────
u32 xadc_read_channel(u8 channel) {
    XSysMon_SetSingleChParams(&xadc_inst,
        channel,
        FALSE,   // no extra acquisition cycles
        FALSE,   // continuous mode
        FALSE);  // unipolar (single-ended)

    // Wait for the exact End-Of-Conversion flag
    while ((XSysMon_GetStatus(&xadc_inst) & XSM_SR_EOC_MASK) != XSM_SR_EOC_MASK);

    return XSysMon_GetAdcData(&xadc_inst, channel);
}

// ── Raw ADC value → voltage (Fixing the 16-bit left-justified quirk) ─────────
float xadc_raw_to_voltage(u32 raw) {
    return ((float)(raw >> 4)) / 4096.0f;
}

// ── Voltage → temperature via LM35 (Reversing the 3.3V board protection) ─────
float voltage_to_temp_c(float voltage) {
    return voltage * 330.0f;
}

// ── Voltage → LDR light level 0–100% ─────────────────────────────────────────
float voltage_to_ldr_percent(float voltage) {
    float pct = voltage * 100.0f;
    if (pct > 100.0f) pct = 100.0f;
    if (pct <   0.0f) pct =   0.0f;
    return pct;
}

// ── Buzzer helpers ────────────────────────────────────────────────────────────
void buzzer_on(void)  { XGpio_DiscreteWrite(&buzzer_gpio, 1, 1); }
void buzzer_off(void) { XGpio_DiscreteWrite(&buzzer_gpio, 1, 0); }

// ─────────────────────────────────────────────────────────────────────────────
int main(void) {
    init_platform();
    xil_printf("\r\n=== EDGE BOARD WI-FI TELEMETRY ===\r\n");

    // ── 1. Initialize XADC ───────────────────────────────────────────────────
    XSysMon_Config *xadc_cfg = XSysMon_LookupConfig(MY_XADC_BASE);
    if (xadc_cfg == NULL) {
        xil_printf("[ERROR] XADC LookupConfig failed\r\n");
        return -1;
    }
    int xadc_status = XSysMon_CfgInitialize(&xadc_inst, xadc_cfg, xadc_cfg->BaseAddress);
    if (xadc_status != XST_SUCCESS) {
        xil_printf("[ERROR] XADC CfgInitialize failed\r\n");
        return -1;
    }
    XSysMon_SetAlarmEnables(&xadc_inst, 0x0);
    XSysMon_GetStatus(&xadc_inst);
    xil_printf("[OK] XADC initialized\r\n");

    // ── 2. Initialize Buzzer GPIO ─────────────────────────────────────────────
    int gpio_status = XGpio_Initialize(&buzzer_gpio, MY_GPIO_BASE);
    if (gpio_status != XST_SUCCESS) {
        xil_printf("[ERROR] GPIO init failed\r\n");
        return -1;
    }
    XGpio_SetDataDirection(&buzzer_gpio, 1, 0x0); // channel 1 = all outputs
    buzzer_off();
    xil_printf("[OK] Buzzer GPIO initialized\r\n");

    // ── 3. Initialize UART ───────────────────────────────────────────────────
    XUartLite_Initialize(&esp32_uart, MY_WIFI_BASE);
    xil_printf("[OK] UART initialized\r\n");

    // ── 4. Connect ESP32 to Wi-Fi ────────────────────────────────────────────
    sleep(2);
    send_at_command("AT",          1000);
    send_at_command("AT+CWMODE=1", 1000);
    xil_printf("Connecting to hotspot...\r\n");
    send_at_command("AT+CWJAP=\"NARZO 70 5G\",\"YogananthGR\"", 8000);

    // ── 5. Open TCP connection ───────────────────────────────────────────────
    xil_printf("Opening TCP connection...\r\n");
    send_at_command("AT+CIPMUX=0", 1000);

    char connect_cmd[80];
    sprintf(connect_cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", PHONE_IP, PHONE_PORT);
    send_at_command(connect_cmd, 3000);

    xil_printf("\r\n[LIVE] Streaming sensor data...\r\n\r\n");

    // ── 6. Sensor read + transmit loop ───────────────────────────────────────
    u32   raw;
    float voltage, temp_c, ldr_pct;
    int   temp_whole, temp_dec, ldr_whole;
    char  status_str[12];
    char  payload[128];
    char  cipsend_cmd[32];
    char *ptr;

    while (1) {

        // -- Read LM35 temperature from Vaux2 --
        raw     = xadc_read_channel(TEMP_CH);
        voltage = xadc_raw_to_voltage(raw);
        temp_c  = voltage_to_temp_c(voltage);

        // -- Read LDR from Vaux10 --
        raw     = xadc_read_channel(LDR_CH);
        voltage = xadc_raw_to_voltage(raw);
        ldr_pct = voltage_to_ldr_percent(voltage);

        // -- Determine status and drive buzzer --
        if (temp_c >= TEMP_CRITICAL_C) {
            sprintf(status_str, "CRITICAL");
            buzzer_on();
        } else {
            sprintf(status_str, "NORMAL");
            buzzer_off();
        }

        // -- Format values --
        temp_whole = (int)temp_c;
        temp_dec   = (int)((temp_c - (float)temp_whole) * 10.0f);
        if (temp_dec < 0) temp_dec = -temp_dec;
        ldr_whole  = (int)ldr_pct;

        // -- Build JSON payload --
        sprintf(payload,
            "{\"Temp\":%d.%d,\"LDR\":%d,\"Status\":\"%s\"}\r\n",
            temp_whole, temp_dec, ldr_whole, status_str);

        xil_printf("Transmitting: %s", payload);

        // -- Send Payload Length --
        sprintf(cipsend_cmd, "AT+CIPSEND=%d", (int)strlen(payload));
        ptr = cipsend_cmd;
        while (*ptr != '\0') {
            XUartLite_SendByte(MY_WIFI_BASE, (u8)*ptr++);
        }
        XUartLite_SendByte(MY_WIFI_BASE, (u8)'\r');
        XUartLite_SendByte(MY_WIFI_BASE, (u8)'\n');

        usleep(200000); // Wait for ESP32 '>' prompt

        // -- Blast the JSON payload over Wi-Fi --
        ptr = payload;
        while (*ptr != '\0') {
            XUartLite_SendByte(MY_WIFI_BASE, (u8)*ptr++);
        }

        sleep(2);
    }

    cleanup_platform();
    return 0;
}