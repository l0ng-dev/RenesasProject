#include "da16200.h"
#include "app_config.h"
#include "wifi_credentials.h"
#include <stdio.h>
#include <string.h>
/* RX callback writes g_rx_head; the foreground parser writes g_rx_tail. */
static uint8_t g_rx_buffer[DA16200_RX_BUFFER_SIZE];
static volatile uint32_t g_rx_head;
static volatile uint32_t g_rx_tail;
static volatile uint32_t g_rx_received;
static volatile uint32_t g_rx_dropped;
static volatile uint32_t g_rx_high_watermark;
static volatile uint32_t g_rx_lines;
static volatile uint32_t g_rx_unsolicited_lines;
static volatile uint32_t g_rx_line_overflows;
static volatile uint32_t g_rx_invalid_bytes;
static volatile uint32_t g_rx_partial_resets;
static volatile uint32_t g_uart_error_events;
static volatile uint32_t g_last_uart_event;
static volatile uint32_t g_at_attempt;
static volatile uint32_t g_wakeup_pulse_count;
static volatile uint32_t g_wakeup_indication_count;
static volatile uint32_t g_wakeup_wait_ms;
static volatile uint32_t g_wakeup_uart_event;
static volatile uint8_t g_wakeup_rx_seen;
/* 0: init, 1: idle high, 2: low pulse, 3: high restored,
 * 4: UART recovery wait, 5: AT pending, 6: AT OK,
 * 7: AT failed, 8: GPIO write failed. */
static volatile uint8_t g_wakeup_stage;
static volatile uint32_t g_wakeup_gpio_result;
/* 0: pending, 1: required response and OK, 2: ERROR, 3: timeout,
 * 4: TX failure, 5: RX buffer full, 6: UART RX error,
 * 7: OK without the required extended response. */
static volatile uint8_t g_at_result;
static volatile uint8_t g_tx_complete;
static volatile uint8_t g_uart_error;
static volatile uint8_t g_at_pending;
static volatile uint8_t g_at_expected_seen;
static volatile uint8_t g_at_sync_result;
/* DPM wake handshake: 0 idle, 1 MCUWUDONE, 2 clear sleep,
 * 3 basic AT, 4 restore sleep, 5 complete, 6 failed. */
static volatile uint8_t g_dpm_handshake_stage;
static volatile uint8_t g_mcuwudone_result;
static volatile uint8_t g_clear_dpm_sleep_result;
static volatile uint8_t g_set_dpm_sleep_result;
static volatile uint8_t g_wifi_status_result;
/* 0: not queried/invalid response, 1: connected, 2: disconnected. */
static volatile uint8_t g_wifi_status;
static volatile uint8_t g_scan_result;
static volatile uint8_t g_scan_active;
static volatile uint8_t g_join_command_result;
/* 0: not attempted/pending, 1: joined, 2: module reported failure,
 * 3: asynchronous result timeout, 4: configured SSID not found by scan. */
static volatile uint8_t g_join_result;
static volatile uint8_t g_join_active;
static volatile uint8_t g_target_ap_found;
static volatile uint32_t g_scan_ap_count;
/* 0: not connected, 1: configured hotspot joined. */
static volatile uint8_t g_wifi_ready;
/* Cooperative Wi-Fi connection diagnostics for Keil Watch. */
volatile uint8_t g_da16200_connect_state;
volatile uint8_t g_da16200_connect_result;
volatile uint32_t g_da16200_connect_elapsed_ms;
volatile uint32_t g_da16200_connect_attempt_count;
volatile uint32_t g_da16200_connect_success_count;
volatile uint32_t g_da16200_connect_failure_count;
volatile uint32_t g_da16200_wifi_offline_ms;
volatile uint32_t g_da16200_wifi_last_offline_ms;
/* MQTT diagnostics are intentionally non-secret and visible in Keil Watch.
 * Credentials and topic are supplied from the ignored local configuration. */
volatile uint8_t g_da16200_mqtt_config_result;
volatile uint8_t g_da16200_mqtt_client_state;
volatile uint8_t g_da16200_mqtt_query_result;
volatile uint8_t g_da16200_mqtt_start_result;
volatile uint32_t g_da16200_mqtt_client_event_count;
volatile uint8_t g_da16200_mqtt_publish_result;
volatile int32_t g_da16200_mqtt_publish_error;
volatile uint32_t g_da16200_mqtt_publish_event_count;
volatile uint32_t g_da16200_mqtt_publish_attempt_count;
volatile uint32_t g_da16200_mqtt_publish_success_count;
volatile uint32_t g_da16200_mqtt_publish_failure_count;
/* Cooperative MQTT transaction diagnostics for Keil Watch.
 * State 0 is idle; non-zero states identify the current network step. */
volatile uint8_t g_da16200_mqtt_service_state;
volatile uint32_t g_da16200_mqtt_service_elapsed_ms;
static volatile uint8_t g_mqtt_publish_event_result;
static bool g_uart_opened;
static bool g_mqtt_config_applied;
static char g_wifi_status_line[DA16200_LINE_SIZE];
static char g_scan_first_line[DA16200_LINE_SIZE];
static volatile char g_target_ap_line[DA16200_LINE_SIZE];
static volatile char g_join_line[DA16200_LINE_SIZE];
/* Preserve a scan ERROR line even if a later unsolicited event arrives. */
static volatile char g_scan_error_line[DA16200_LINE_SIZE];
/* Volatile keeps the latest complete response line visible in Keil Watch. */
static volatile char g_last_rx_line[DA16200_LINE_SIZE];
static char g_rx_line[DA16200_LINE_SIZE];
static uint32_t g_rx_line_length;
static bool g_rx_line_discarding;
static char const * gp_at_expected_prefix;
static char * gp_at_response_line;
static size_t g_at_response_line_size;
static char g_mqtt_status_line[DA16200_LINE_SIZE];
static char g_mqtt_publish_command[DA16200_LINE_SIZE];
static char g_mqtt_control_command[DA16200_LINE_SIZE];
static char g_async_at_command[DA16200_LINE_SIZE];
static uint32_t g_async_at_timeout_ms;
static uint32_t g_async_at_tx_elapsed_ms;
static uint32_t g_async_at_response_elapsed_ms;
static uint32_t g_async_at_dropped_before;
static uint32_t g_mqtt_wakeup_indication_before;
static uint32_t g_mqtt_client_event_before;
static uint32_t g_mqtt_publish_event_before;
static uint32_t g_mqtt_state_elapsed_ms;
static uint8_t g_mqtt_pending_result;
static bool g_async_at_active;
static bool g_mqtt_dpm_session_active;
static bool g_mqtt_result_ready;
static uint32_t g_connect_wakeup_indication_before;
static uint32_t g_connect_state_elapsed_ms;
static uint8_t g_connect_pending_result;
static bool g_connect_dpm_restore_needed;

enum
{
    DA16200_CONNECT_STATE_IDLE = 0U,
    DA16200_CONNECT_STATE_POWER_ON_WAIT,
    DA16200_CONNECT_STATE_WAIT_WAKE,
    DA16200_CONNECT_STATE_MCU_WAKE_DONE,
    DA16200_CONNECT_STATE_CLEAR_DPM_SLEEP,
    DA16200_CONNECT_STATE_SYNC,
    DA16200_CONNECT_STATE_WIFI_QUERY,
    DA16200_CONNECT_STATE_SCAN,
    DA16200_CONNECT_STATE_JOIN_COMMAND,
    DA16200_CONNECT_STATE_JOIN_WAIT,
    DA16200_CONNECT_STATE_RESTORE_DPM_SLEEP
};

enum
{
    DA16200_CONNECT_RESULT_NOT_ATTEMPTED = 0U,
    DA16200_CONNECT_RESULT_OK,
    DA16200_CONNECT_RESULT_UART_OPEN_FAILED,
    DA16200_CONNECT_RESULT_WAKE_FAILED,
    DA16200_CONNECT_RESULT_DPM_HANDSHAKE_FAILED,
    DA16200_CONNECT_RESULT_SYNC_FAILED,
    DA16200_CONNECT_RESULT_WIFI_QUERY_FAILED,
    DA16200_CONNECT_RESULT_SCAN_FAILED,
    DA16200_CONNECT_RESULT_AP_NOT_FOUND,
    DA16200_CONNECT_RESULT_JOIN_COMMAND_FAILED,
    DA16200_CONNECT_RESULT_JOIN_FAILED,
    DA16200_CONNECT_RESULT_DPM_RESTORE_FAILED
};

enum
{
    DA16200_MQTT_STATE_IDLE = 0U,
    DA16200_MQTT_STATE_WAIT_WAKE,
    DA16200_MQTT_STATE_MCU_WAKE_DONE,
    DA16200_MQTT_STATE_CLEAR_DPM_SLEEP,
    DA16200_MQTT_STATE_SYNC,
    DA16200_MQTT_STATE_WIFI_QUERY,
    DA16200_MQTT_STATE_CONFIG_STOP,
    DA16200_MQTT_STATE_CONFIG_BROKER,
    DA16200_MQTT_STATE_CONFIG_SUB_TOPIC,
    DA16200_MQTT_STATE_CONFIG_PUB_TOPIC,
    DA16200_MQTT_STATE_CONFIG_CLIENT_ID,
    DA16200_MQTT_STATE_CONFIG_QOS,
    DA16200_MQTT_STATE_CONFIG_AUTO,
    DA16200_MQTT_STATE_MQTT_QUERY,
    DA16200_MQTT_STATE_MQTT_START,
    DA16200_MQTT_STATE_MQTT_CONNECT_WAIT,
    DA16200_MQTT_STATE_PUBLISH_COMMAND,
    DA16200_MQTT_STATE_PUBLISH_ACK_WAIT,
    DA16200_MQTT_STATE_RESTORE_DPM_SLEEP
};

static bool da16200_is_sensitive_command_echo (char const * p_line)
{
    return ((0 == strncmp(p_line, "AT+WFJAPA=", 10U)) ||
            (0 == strncmp(p_line, "AT+NWMQTS=", 10U)) ||
            (0 == strncmp(p_line, "AT+NWMQTP=", 10U)) ||
            (0 == strncmp(p_line, "AT+NWMQCID=", 11U)));
}

static bool da16200_is_hex_digit (char value)
{
    return (((value >= '0') && (value <= '9')) ||
            ((value >= 'A') && (value <= 'F')) ||
            ((value >= 'a') && (value <= 'f')));
}

static int32_t da16200_parse_signed_decimal (char const * p_text)
{
    int32_t value = 0;
    int32_t sign = 1;

    if (NULL == p_text)
    {
        return 0;
    }

    if ('-' == *p_text)
    {
        sign = -1;
        p_text++;
    }

    while ((*p_text >= '0') && (*p_text <= '9'))
    {
        value = (value * 10) + (int32_t) (*p_text - '0');
        p_text++;
    }

    return value * sign;
}

static bool da16200_looks_like_bssid (char const * p_text)
{
    if ((NULL == p_text) || (strlen(p_text) < 17U))
    {
        return false;
    }

    for (uint32_t index = 0U; index < 17U; index++)
    {
        if (((2U == index) || (5U == index) || (8U == index) ||
             (11U == index) || (14U == index)))
        {
            if (':' != p_text[index])
            {
                return false;
            }
        }
        else if (!da16200_is_hex_digit(p_text[index]))
        {
            return false;
        }
    }

    return true;
}

void user_uart_callback (uart_callback_args_t * p_args)
{
    if (NULL == p_args)
    {
        return;
    }

    if (p_args->event & (UART_EVENT_ERR_PARITY | UART_EVENT_ERR_FRAMING |
                         UART_EVENT_ERR_OVERFLOW | UART_EVENT_BREAK_DETECT))
    {
        g_last_uart_event |= (uint32_t) p_args->event;
        g_uart_error_events++;
    }

    switch (p_args->event)
    {
        case UART_EVENT_RX_CHAR:
        {
            uint32_t next = (g_rx_head + 1U) % DA16200_RX_BUFFER_SIZE;

            if (next != g_rx_tail)
            {
                uint32_t used;

                g_rx_buffer[g_rx_head] = (uint8_t) p_args->data;
                g_rx_head = next;
                g_rx_received++;
                used = (next + DA16200_RX_BUFFER_SIZE - g_rx_tail) % DA16200_RX_BUFFER_SIZE;
                if (used > g_rx_high_watermark)
                {
                    g_rx_high_watermark = used;
                }
            }
            else
            {
                g_rx_dropped++;
            }
            break;
        }

        case UART_EVENT_TX_COMPLETE:
            g_tx_complete = 1U;
            break;

        default:
            break;
    }
}

static bool da16200_read_byte (uint8_t * p_data)
{
    uint32_t tail = g_rx_tail;

    if (tail == g_rx_head)
    {
        return false;
    }

    *p_data = g_rx_buffer[tail];
    g_rx_tail = (tail + 1U) % DA16200_RX_BUFFER_SIZE;
    return true;
}

static void da16200_check_line (char const * p_line)
{
    static char const mqtt_client_prefix[] = "+NWMQCL:";
    static char const mqtt_publish_prefix[] = "+NWMQMSGSND:";
    size_t length = strlen(p_line);
    char const * p_scan_line = p_line;
    size_t const mqtt_client_prefix_length = sizeof(mqtt_client_prefix) - 1U;
    size_t const mqtt_publish_prefix_length = sizeof(mqtt_publish_prefix) - 1U;

    if (da16200_is_sensitive_command_echo(p_line))
    {
        static char const redacted[] = "<redacted sensitive command echo>";
        for (size_t index = 0U; index < sizeof(redacted); index++)
        {
            g_last_rx_line[index] = redacted[index];
        }
    }
    else
    {
        for (size_t index = 0U; index <= length; index++)
        {
            g_last_rx_line[index] = p_line[index];
        }
    }
    g_rx_lines++;

    if ((length > mqtt_client_prefix_length) &&
        (0 == strncmp(p_line, mqtt_client_prefix, mqtt_client_prefix_length)))
    {
        char const state = p_line[mqtt_client_prefix_length];

        if ((state >= '0') && (state <= '2'))
        {
            if (length < sizeof(g_mqtt_status_line))
            {
                memcpy(g_mqtt_status_line, p_line, length + 1U);
            }
            g_da16200_mqtt_client_state = (uint8_t) (state - '0');
            g_da16200_mqtt_client_event_count++;
        }
    }

    if ((length > mqtt_publish_prefix_length) &&
        (0 == strncmp(p_line, mqtt_publish_prefix, mqtt_publish_prefix_length)))
    {
        if (('1' == p_line[mqtt_publish_prefix_length]) &&
            ('\0' == p_line[mqtt_publish_prefix_length + 1U]))
        {
            g_mqtt_publish_event_result = DA16200_MQTT_PUBLISH_OK;
            g_da16200_mqtt_publish_error = 0;
        }
        else
        {
            char const * p_error = strchr(p_line + mqtt_publish_prefix_length, ',');

            g_mqtt_publish_event_result = DA16200_MQTT_PUBLISH_BROKER_REJECTED;
            g_da16200_mqtt_publish_error = (NULL != p_error) ?
                                             da16200_parse_signed_decimal(p_error + 1U) : 0;
        }
        g_da16200_mqtt_publish_event_count++;
    }

    /* The leading characters can be damaged by the wake-time framing/break
     * event. The complete suffix plus the line terminator is sufficient to
     * identify DA16200's external wake indication. */
    size_t const wake_suffix_length = sizeof(DA16200_WAKE_INDICATION_SUFFIX) - 1U;
    if ((length >= wake_suffix_length) &&
        (0 == strcmp(p_line + length - wake_suffix_length,
                     DA16200_WAKE_INDICATION_SUFFIX)))
    {
        g_wakeup_indication_count++;
    }

    if ((0U != g_join_active) && (0 == strncmp(p_line, "+WFJAP:", 7U)))
    {
        for (size_t index = 0U; index <= length; index++)
        {
            g_join_line[index] = p_line[index];
        }

        g_join_result = (0 == strncmp(p_line, "+WFJAP:1,", 9U)) ? 1U : 2U;
        g_join_active = 0U;
    }

    if (0U == g_at_pending)
    {
        g_rx_unsolicited_lines++;
        return;
    }

    if (0U != g_at_result)
    {
        g_rx_unsolicited_lines++;
        return;
    }

    if (0U != g_scan_active)
    {
        if (0 == strncmp(p_line, "+WFSCAN:", 8U))
        {
            p_scan_line = p_line + 8U;
        }

        if (da16200_looks_like_bssid(p_scan_line))
        {
            char const * p_ssid;

            g_scan_ap_count++;
            if ('\0' == g_scan_first_line[0])
            {
                memcpy(g_scan_first_line, p_line, length + 1U);
            }

            p_ssid = strrchr(p_scan_line, '\t');
            if ((NULL != p_ssid) && (0 == strcmp(p_ssid + 1, WIFI_SSID)))
            {
                g_target_ap_found = 1U;
                for (size_t index = 0U; index <= length; index++)
                {
                    g_target_ap_line[index] = p_line[index];
                }
            }
        }
    }

    if ((NULL != gp_at_expected_prefix) &&
        (0 == strncmp(p_line, gp_at_expected_prefix, strlen(gp_at_expected_prefix))))
    {
        if ((NULL != gp_at_response_line) && (length < g_at_response_line_size))
        {
            memcpy(gp_at_response_line, p_line, length + 1U);
            g_at_expected_seen = 1U;
        }
        return;
    }

    if (0 == strcmp(p_line, "OK"))
    {
        g_at_result = ((NULL == gp_at_expected_prefix) || (0U != g_at_expected_seen)) ? 1U : 7U;
        return;
    }

    if (0 == strncmp(p_line, "ERROR", 5U))
    {
        if (0U != g_scan_active)
        {
            for (size_t index = 0U; index <= length; index++)
            {
                g_scan_error_line[index] = p_line[index];
            }
        }
        g_at_result = 2U;
    }
}

static void da16200_process_rx (void)
{
    uint8_t byte;

    while (da16200_read_byte(&byte))
    {
        if (('\r' == byte) || ('\n' == byte))
        {
            if (g_rx_line_discarding)
            {
                g_rx_line_discarding = false;
                g_rx_line_length = 0U;
            }
            else if (g_rx_line_length > 0U)
            {
                g_rx_line[g_rx_line_length] = '\0';
                da16200_check_line(g_rx_line);
                g_rx_line_length = 0U;
            }
        }
        /* AT+WFSCAN separates fields with TAB; keep it as valid line data. */
        else if (('\t' == byte) || ((byte >= 0x20U) && (byte <= 0x7EU)))
        {
            if (!g_rx_line_discarding)
            {
                if (g_rx_line_length < (sizeof(g_rx_line) - 1U))
                {
                    g_rx_line[g_rx_line_length++] = (char) byte;
                }
                else
                {
                    g_rx_line_discarding = true;
                    g_rx_line_length = 0U;
                    g_rx_line_overflows++;
                }
            }
        }
        else
        {
            g_rx_invalid_bytes++;
            g_rx_line_discarding = true;
            g_rx_line_length = 0U;
        }
    }
}

static void da16200_service_delay (uint32_t delay_ms)
{
    while (delay_ms--)
    {
        da16200_process_rx();
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }
}

static bool da16200_wakeup_pulse (void)
{
    fsp_err_t err;

    g_wakeup_stage = 1U;
    err = R_IOPORT_PinWrite(&g_ioport_ctrl,
                            BSP_IO_PORT_01_PIN_02,
                            BSP_IO_LEVEL_HIGH);
    if (FSP_SUCCESS != err)
    {
        g_wakeup_gpio_result = (uint32_t) err;
        g_wakeup_stage = 8U;
        return false;
    }

    g_wakeup_stage = 2U;
    err = R_IOPORT_PinWrite(&g_ioport_ctrl,
                            BSP_IO_PORT_01_PIN_02,
                            BSP_IO_LEVEL_LOW);
    if (FSP_SUCCESS != err)
    {
        g_wakeup_gpio_result = (uint32_t) err;
        g_wakeup_stage = 8U;
        return false;
    }

    da16200_service_delay(DA16200_WAKE_PULSE_MS);

    err = R_IOPORT_PinWrite(&g_ioport_ctrl,
                            BSP_IO_PORT_01_PIN_02,
                            BSP_IO_LEVEL_HIGH);
    if (FSP_SUCCESS != err)
    {
        g_wakeup_gpio_result = (uint32_t) err;
        g_wakeup_stage = 8U;
        return false;
    }

    g_wakeup_gpio_result = (uint32_t) FSP_SUCCESS;
    g_wakeup_pulse_count++;
    g_wakeup_stage = 3U;
    return true;
}

static uint8_t da16200_execute_command (char const * p_command,
                                        char const * p_expected_prefix,
                                        char * p_response_line,
                                        size_t response_line_size,
                                        uint32_t timeout_ms)
{
    uint32_t dropped_before;
    size_t command_length;

    if ((NULL == p_command) || (0U == timeout_ms) ||
        ((NULL != p_expected_prefix) &&
         ((NULL == p_response_line) || (response_line_size < 2U))))
    {
        return 4U;
    }

    command_length = strlen(p_command);
    if (0U == command_length)
    {
        return 4U;
    }

    g_at_attempt++;
    da16200_process_rx(); /* Preserve complete start-up and asynchronous lines. */
    if ((g_rx_line_length > 0U) || g_rx_line_discarding)
    {
        /* Do not let an incomplete asynchronous line prefix the command response. */
        g_rx_partial_resets++;
        g_rx_line_length = 0U;
        g_rx_line_discarding = false;
    }

    gp_at_expected_prefix = p_expected_prefix;
    gp_at_response_line = p_response_line;
    g_at_response_line_size = response_line_size;
    g_at_expected_seen = 0U;
    if ((NULL != p_response_line) && (response_line_size > 0U))
    {
        p_response_line[0] = '\0';
    }
    g_last_uart_event = 0U;
    dropped_before = g_rx_dropped;
    g_tx_complete = 0U;
    g_at_result = 0U;
    g_at_pending = 1U;

    /* Keep the source buffer valid until UART_EVENT_TX_COMPLETE. */
    if (FSP_SUCCESS != g_uart0.p_api->write(g_uart0.p_ctrl,
                                            (uint8_t const *) p_command,
                                            (uint32_t) command_length))
    {
        g_at_result = 4U;
        g_at_pending = 0U;
        return g_at_result;
    }

    uint32_t tx_timeout = 100U;
    while ((0U == g_tx_complete) && tx_timeout--)
    {
        da16200_process_rx();
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    if (0U == g_tx_complete)
    {
        g_at_result = 4U;
        g_at_pending = 0U;
        return g_at_result;
    }

    for (uint32_t elapsed = 0U; elapsed < timeout_ms; elapsed++)
    {
        da16200_process_rx();

        if (0U != g_at_result)
        {
            break;
        }

        if (g_rx_dropped != dropped_before)
        {
            g_at_result = 5U;
            break;
        }

        if (0U != g_last_uart_event)
        {
            g_at_result = 6U;
            break;
        }

        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    if (0U == g_at_result)
    {
        g_at_result = 3U;
    }
    g_at_pending = 0U;
    return g_at_result;
}

/* Start one AT transaction without waiting for TX completion or a response.
 * The command is copied because the FSP UART driver requires its source
 * buffer to remain valid until UART_EVENT_TX_COMPLETE. */
static bool da16200_async_command_start (char const * p_command,
                                         char const * p_expected_prefix,
                                         char * p_response_line,
                                         size_t response_line_size,
                                         uint32_t timeout_ms)
{
    size_t command_length;

    if (g_async_at_active || (NULL == p_command) || (0U == timeout_ms) ||
        ((NULL != p_expected_prefix) &&
         ((NULL == p_response_line) || (response_line_size < 2U))))
    {
        return false;
    }

    command_length = strlen(p_command);
    if ((0U == command_length) || (command_length >= sizeof(g_async_at_command)))
    {
        return false;
    }

    da16200_process_rx();
    if ((g_rx_line_length > 0U) || g_rx_line_discarding)
    {
        g_rx_partial_resets++;
        g_rx_line_length = 0U;
        g_rx_line_discarding = false;
    }

    memcpy(g_async_at_command, p_command, command_length + 1U);
    gp_at_expected_prefix = p_expected_prefix;
    gp_at_response_line = p_response_line;
    g_at_response_line_size = response_line_size;
    g_at_expected_seen = 0U;
    if ((NULL != p_response_line) && (response_line_size > 0U))
    {
        p_response_line[0] = '\0';
    }

    g_at_attempt++;
    g_last_uart_event = 0U;
    g_async_at_dropped_before = g_rx_dropped;
    g_async_at_timeout_ms = timeout_ms;
    g_async_at_tx_elapsed_ms = 0U;
    g_async_at_response_elapsed_ms = 0U;
    g_tx_complete = 0U;
    g_at_result = 0U;
    g_at_pending = 1U;
    g_async_at_active = true;

    if (FSP_SUCCESS != g_uart0.p_api->write(g_uart0.p_ctrl,
                                             (uint8_t const *) g_async_at_command,
                                             (uint32_t) command_length))
    {
        g_at_result = 4U;
        g_at_pending = 0U;
        g_async_at_active = false;
        return false;
    }

    return true;
}

/* Return 0 while pending, otherwise the same result codes used by the
 * blocking command helper above. Call exactly once per application tick. */
static uint8_t da16200_async_command_poll (void)
{
    uint8_t result;

    if (!g_async_at_active)
    {
        return 4U;
    }

    da16200_process_rx();
    if (0U == g_tx_complete)
    {
        g_async_at_tx_elapsed_ms++;
        if (g_async_at_tx_elapsed_ms >= 100U)
        {
            g_at_result = 4U;
        }
    }
    else if (0U == g_at_result)
    {
        if (g_rx_dropped != g_async_at_dropped_before)
        {
            g_at_result = 5U;
        }
        else if (0U != g_last_uart_event)
        {
            g_at_result = 6U;
        }
        else
        {
            g_async_at_response_elapsed_ms++;
            if (g_async_at_response_elapsed_ms >= g_async_at_timeout_ms)
            {
                g_at_result = 3U;
            }
        }
    }

    if ((0U == g_tx_complete) || (0U == g_at_result))
    {
        return 0U;
    }

    result = g_at_result;
    g_at_pending = 0U;
    g_async_at_active = false;
    return result;
}

static uint8_t da16200_async_command_step (char const * p_command,
                                            char const * p_expected_prefix,
                                            char * p_response_line,
                                            size_t response_line_size,
                                            uint32_t timeout_ms)
{
    if (!g_async_at_active &&
        !da16200_async_command_start(p_command,
                                     p_expected_prefix,
                                     p_response_line,
                                     response_line_size,
                                     timeout_ms))
    {
        return 4U;
    }

    return da16200_async_command_poll();
}

static uint8_t da16200_mqtt_command_step (char const * p_command,
                                           char const * p_expected_prefix,
                                           char * p_response_line,
                                           size_t response_line_size)
{
    return da16200_async_command_step(p_command,
                                      p_expected_prefix,
                                      p_response_line,
                                      response_line_size,
                                      DA16200_RESPONSE_TIMEOUT_MS);
}

static uint8_t da16200_sync (void)
{
    static char const command[] = "AT\r\n";

    return da16200_execute_command(command,
                                   NULL,
                                   NULL,
                                   0U,
                                   DA16200_RESPONSE_TIMEOUT_MS);
}

/* These commands coordinate one DPM wake session only. They do not change
 * the saved Wi-Fi profile, country code, DPM enable state, or NVRAM. */
static uint8_t da16200_notify_mcu_wakeup_done (void)
{
    static char const command[] = "AT+MCUWUDONE\r\n";

    return da16200_execute_command(command,
                                   NULL,
                                   NULL,
                                   0U,
                                   DA16200_RESPONSE_TIMEOUT_MS);
}

static uint8_t da16200_clear_dpm_sleep_ext (void)
{
    static char const command[] = "AT+CLRDPMSLPEXT\r\n";

    return da16200_execute_command(command,
                                   NULL,
                                   NULL,
                                   0U,
                                   DA16200_RESPONSE_TIMEOUT_MS);
}

static uint8_t da16200_set_dpm_sleep_ext (void)
{
    static char const command[] = "AT+SETDPMSLPEXT\r\n";

    return da16200_execute_command(command,
                                   NULL,
                                   NULL,
                                   0U,
                                   DA16200_RESPONSE_TIMEOUT_MS);
}

static uint8_t da16200_query_wifi_status (void)
{
    static char const command[] = "AT+WFSTA\r\n";
    uint8_t result;

    g_wifi_status = 0U;
    result = da16200_execute_command(command,
                                     "+WFSTA:",
                                     g_wifi_status_line,
                                     sizeof(g_wifi_status_line),
                                     DA16200_RESPONSE_TIMEOUT_MS);

    if (1U == result)
    {
        if (0 == strcmp(g_wifi_status_line, "+WFSTA:1"))
        {
            g_wifi_status = 1U;
        }
        else if (0 == strcmp(g_wifi_status_line, "+WFSTA:0"))
        {
            g_wifi_status = 2U;
        }
        else
        {
            /* Preserve the unexpected response for Watch and use the normal
             * reconnect path instead of treating it as a valid state. */
            result = 7U;
        }
    }

    return result;
}

static bool da16200_mqtt_field_valid (char const * p_text)
{
    size_t length;

    if (NULL == p_text)
    {
        return false;
    }

    length = strlen(p_text);
    if ((0U == length) || (length > DA16200_MQTT_FIELD_MAX_LENGTH))
    {
        return false;
    }

    return ((NULL == strchr(p_text, '\r')) &&
            (NULL == strchr(p_text, '\n')) &&
            (NULL == strchr(p_text, ',')) &&
            (NULL == strchr(p_text, '\'')));
}

static uint8_t da16200_execute_mqtt_config_command (char const * p_format,
                                                     char const * p_text,
                                                     uint32_t number)
{
    char command[DA16200_LINE_SIZE];
    int length;

    if ((NULL == p_format) || (NULL == p_text))
    {
        return 4U;
    }

    length = snprintf(command,
                      sizeof(command),
                      p_format,
                      p_text,
                      (unsigned long) number);
    if ((length <= 0) || ((size_t) length >= sizeof(command)))
    {
        return 4U;
    }

    return da16200_execute_command(command,
                                   NULL,
                                   NULL,
                                   0U,
                                   DA16200_RESPONSE_TIMEOUT_MS);
}

static uint8_t da16200_apply_mqtt_config (void)
{
    static char const stop_command[] = "AT+NWMQCL=0\r\n";
    static char const qos_command[] = "AT+NWMQQOS=0\r\n";
    static char const auto_command[] = "AT+NWMQAUTO=1\r\n";

    g_mqtt_config_applied = false;
    if (!da16200_mqtt_field_valid(DA16200_MQTT_BROKER_HOST) ||
        !da16200_mqtt_field_valid(BEMFA_MQTT_TOPIC) ||
        !da16200_mqtt_field_valid(BEMFA_MQTT_PRIVATE_KEY) ||
        (0U == DA16200_MQTT_BROKER_PORT))
    {
        return 2U;
    }

    if (1U != da16200_execute_command(stop_command, NULL, NULL, 0U,
                                       DA16200_RESPONSE_TIMEOUT_MS))
    {
        return 3U;
    }
    if (1U != da16200_execute_mqtt_config_command("AT+NWMQBR=%s,%lu\r\n",
                                                   DA16200_MQTT_BROKER_HOST,
                                                   DA16200_MQTT_BROKER_PORT))
    {
        return 4U;
    }
    if (1U != da16200_execute_mqtt_config_command("AT+NWMQTS=1,%s\r\n",
                                                   BEMFA_MQTT_TOPIC,
                                                   0U))
    {
        return 5U;
    }
    if (1U != da16200_execute_mqtt_config_command("AT+NWMQTP=%s\r\n",
                                                   BEMFA_MQTT_TOPIC,
                                                   0U))
    {
        return 6U;
    }
    if (1U != da16200_execute_mqtt_config_command("AT+NWMQCID=%s\r\n",
                                                   BEMFA_MQTT_PRIVATE_KEY,
                                                   0U))
    {
        return 7U;
    }
    if (1U != da16200_execute_command(qos_command, NULL, NULL, 0U,
                                       DA16200_RESPONSE_TIMEOUT_MS))
    {
        return 8U;
    }
    if (1U != da16200_execute_command(auto_command, NULL, NULL, 0U,
                                       DA16200_RESPONSE_TIMEOUT_MS))
    {
        return 9U;
    }

    g_mqtt_config_applied = true;
    return 1U;
}

static uint8_t da16200_query_mqtt_status (void)
{
    static char const command[] = "AT+NWMQCL\r\n";
    uint8_t result;

    g_da16200_mqtt_client_state = 0U;
    g_mqtt_status_line[0] = '\0';
    result = da16200_execute_command(command,
                                     "+NWMQCL:",
                                     g_mqtt_status_line,
                                     sizeof(g_mqtt_status_line),
                                     DA16200_RESPONSE_TIMEOUT_MS);

    g_da16200_mqtt_query_result = result;
    return result;
}

/* Start the MQTT client only when the saved DA16200 configuration is idle.
 * This command does not write broker, topic, client ID, or credentials. */
static uint8_t da16200_start_mqtt_client (void)
{
    static char const command[] = "AT+NWMQCL=1\r\n";
    uint32_t const event_before = g_da16200_mqtt_client_event_count;
    uint8_t const command_result = da16200_execute_command(command,
                                                           NULL,
                                                           NULL,
                                                           0U,
                                                           DA16200_RESPONSE_TIMEOUT_MS);

    if (1U != command_result)
    {
        return 4U;
    }

    for (uint32_t elapsed = 0U; elapsed < DA16200_MQTT_CONNECT_TIMEOUT_MS; elapsed++)
    {
        da16200_process_rx();
        if (1U == g_da16200_mqtt_client_state)
        {
            return 1U;
        }
        if ((g_da16200_mqtt_client_event_count != event_before) &&
            (0U == g_da16200_mqtt_client_state))
        {
            return 2U;
        }
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    return 3U;
}

static uint8_t da16200_wait_for_mqtt_connection (void)
{
    uint32_t const event_before = g_da16200_mqtt_client_event_count;

    for (uint32_t elapsed = 0U; elapsed < DA16200_MQTT_CONNECT_TIMEOUT_MS; elapsed++)
    {
        da16200_process_rx();
        if (1U == g_da16200_mqtt_client_state)
        {
            return 1U;
        }
        if ((g_da16200_mqtt_client_event_count != event_before) &&
            (0U == g_da16200_mqtt_client_state))
        {
            return 2U;
        }
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    return 3U;
}

static uint8_t da16200_scan_access_points (void)
{
    static char const command[] = "AT+WFSCAN\r\n";
    uint8_t result;

    g_scan_ap_count = 0U;
    g_scan_first_line[0] = '\0';
    g_scan_error_line[0] = '\0';
    g_target_ap_found = 0U;
    g_target_ap_line[0] = '\0';
    g_scan_active = 1U;
    result = da16200_execute_command(command,
                                     "+WFSCAN:",
                                     g_scan_first_line,
                                     sizeof(g_scan_first_line),
                                     DA16200_SCAN_TIMEOUT_MS);
    g_scan_active = 0U;
    return result;
}

static void da16200_join_configured_ap (void)
{
    static char const command[] = "AT+WFJAPA=" WIFI_SSID "," WIFI_PASSWORD "\r\n";

    g_join_command_result = 0U;
    g_join_result = 0U;
    g_join_line[0] = '\0';
    g_join_active = 1U;

    g_join_command_result = da16200_execute_command(command,
                                                    NULL,
                                                    NULL,
                                                    0U,
                                                    DA16200_RESPONSE_TIMEOUT_MS);
    if (1U != g_join_command_result)
    {
        g_join_active = 0U;
        return;
    }

    for (uint32_t elapsed = 0U;
         (elapsed < DA16200_JOIN_TIMEOUT_MS) && (0U == g_join_result);
         elapsed++)
    {
        da16200_process_rx();
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    if (0U == g_join_result)
    {
        g_join_result = 3U;
    }
    g_join_active = 0U;
}

static bool da16200_connect_configured_hotspot (void)
{
    g_wifi_ready = 0U;
    g_wifi_status_result = da16200_query_wifi_status();
    if ((1U == g_wifi_status_result) && (1U == g_wifi_status))
    {
        /* DA16200 can restore its saved AP profile before the RA host starts.
         * Accept that real connection instead of forcing a scan/rejoin. */
        g_wifi_ready = 1U;
        return true;
    }

    g_scan_result = da16200_scan_access_points();
    if (1U != g_scan_result)
    {
        return false;
    }

    if (0U == g_target_ap_found)
    {
        /* Distinguish an unavailable SSID from an AT rejection or timeout. */
        g_join_result = 4U;
        return false;
    }

    da16200_join_configured_ap();
    if (1U != g_join_result)
    {
        return false;
    }

    g_wifi_ready = 1U;
    return true;
}
/*******************************************************************************************************************//**
 * main() is generated by the RA Configuration editor and is used to generate threads if an RTOS is used.  This function
 * is called by main() when no RTOS is used.
 **********************************************************************************************************************/
bool DA16200_Connect (void)
{
    fsp_err_t err;

    if (!g_uart_opened)
    {
        g_rx_head = 0U;
        g_rx_tail = 0U;
        g_rx_received = 0U;
        g_rx_dropped = 0U;
        g_rx_high_watermark = 0U;
        g_rx_lines = 0U;
        g_rx_unsolicited_lines = 0U;
        g_rx_line_overflows = 0U;
        g_rx_invalid_bytes = 0U;
        g_rx_partial_resets = 0U;
        g_uart_error_events = 0U;
        g_wakeup_pulse_count = 0U;
        g_wakeup_indication_count = 0U;
        g_at_attempt = 0U;
        g_da16200_mqtt_config_result = 0U;
        g_mqtt_config_applied = false;
        g_da16200_mqtt_service_state = DA16200_MQTT_STATE_IDLE;
        g_da16200_mqtt_service_elapsed_ms = 0U;
        g_async_at_active = false;
        g_mqtt_dpm_session_active = false;
        g_mqtt_result_ready = false;

        err = g_uart0.p_api->open(g_uart0.p_ctrl, g_uart0.p_cfg);
        if (FSP_SUCCESS != err)
        {
            g_at_result = 4U;
            g_uart_error = 1U;
            return false;
        }
        g_uart_opened = true;

        /* Keep UART TX idle while the DA16200 bootloader starts FRTOS. */
        da16200_service_delay(DA16200_POWER_ON_DELAY_MS);
    }

    da16200_process_rx();
    g_wakeup_wait_ms = 0U;
    g_wakeup_uart_event = 0U;
    g_wakeup_rx_seen = 0U;
    g_wakeup_stage = 0U;
    g_wakeup_gpio_result = (uint32_t) FSP_SUCCESS;
    g_rx_line_length = 0U;
    g_rx_line_discarding = false;
    g_tx_complete = 0U;
    g_uart_error = 0U;
    g_at_pending = 0U;
    g_at_expected_seen = 0U;
    g_at_sync_result = 0U;
    g_dpm_handshake_stage = 0U;
    g_mcuwudone_result = 0U;
    g_clear_dpm_sleep_result = 0U;
    g_set_dpm_sleep_result = 0U;
    g_wifi_status_result = 0U;
    g_wifi_status = 0U;
    g_scan_result = 0U;
    g_scan_active = 0U;
    g_join_command_result = 0U;
    g_join_result = 0U;
    g_join_active = 0U;
    g_target_ap_found = 0U;
    g_scan_ap_count = 0U;
    g_wifi_ready = 0U;
    g_wifi_status_line[0] = '\0';
    g_scan_first_line[0] = '\0';
    g_target_ap_line[0] = '\0';
    g_join_line[0] = '\0';
    g_scan_error_line[0] = '\0';
    g_last_rx_line[0] = '\0';
    gp_at_expected_prefix = NULL;
    gp_at_response_line = NULL;
    g_at_response_line_size = 0U;

    /* Prefer the validated RTC_WAKE_UP path. If the module is still awake
     * after a cold start, fall back to a normal AT session and reconnect
     * the configured hotspot without changing DPM or country settings. */
    uint32_t wakeup_indication_before = g_wakeup_indication_count;

    if (da16200_wakeup_pulse())
    {
        g_wakeup_stage = 4U;
        g_wakeup_wait_ms = 0U;
        g_wakeup_uart_event = 0U;
        g_wakeup_rx_seen = 0U;
        for (uint32_t elapsed = 0U; elapsed < DA16200_WAKE_RX_MAX_WAIT_MS; elapsed++)
        {
            da16200_process_rx();
            if (g_wakeup_indication_count != wakeup_indication_before)
            {
                g_wakeup_rx_seen = 1U;
                g_wakeup_wait_ms = elapsed + 1U;
                break;
            }

            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
            g_wakeup_wait_ms = elapsed + 1U;
        }

        /* A sleeping DA16200 can briefly hold TX low while its UART
         * resumes, which FSP reports as framing + break. Preserve that
         * wake-phase event for Watch, then judge only new errors from
         * the following AT transaction. */
        g_wakeup_uart_event = g_last_uart_event;
        g_last_uart_event = 0U;
        if (0U != g_wakeup_rx_seen)
        {
            g_wakeup_stage = 5U;
            g_dpm_handshake_stage = 1U;
            g_mcuwudone_result = da16200_notify_mcu_wakeup_done();

            if (1U == g_mcuwudone_result)
            {
                g_dpm_handshake_stage = 2U;
                g_clear_dpm_sleep_result = da16200_clear_dpm_sleep_ext();
            }

            if (1U == g_clear_dpm_sleep_result)
            {
                g_dpm_handshake_stage = 3U;
                g_at_sync_result = da16200_sync();

                if (1U == g_at_sync_result)
                {
                    (void) da16200_connect_configured_hotspot();
                }

                /* Restore normal DPM entry after the Wi-Fi check. */
                g_dpm_handshake_stage = 4U;
                g_set_dpm_sleep_result = da16200_set_dpm_sleep_ext();
            }

            if ((1U == g_mcuwudone_result) &&
                (1U == g_clear_dpm_sleep_result) &&
                (1U == g_at_sync_result) &&
                (1U == g_set_dpm_sleep_result) &&
                (1U == g_wifi_ready))
            {
                g_dpm_handshake_stage = 5U;
                g_wakeup_stage = 6U;
            }
            else
            {
                g_dpm_handshake_stage = 6U;
                g_wakeup_stage = 7U;
            }
        }
        else
        {
            /* A cold-started or already-awake module does not emit the
             * DPM wake indication. Try the normal AT path so hotspot
             * recovery is not blocked waiting for Power-Down state. */
            g_at_sync_result = da16200_sync();
            if (1U == g_at_sync_result)
            {
                (void) da16200_connect_configured_hotspot();
            }

            if (1U == g_wifi_ready)
            {
                g_wakeup_stage = 6U;
            }
            else
            {
                g_dpm_handshake_stage = 6U;
                g_wakeup_stage = 7U;
            }
        }
    }
    else
    {
        g_at_sync_result = 4U;
        g_dpm_handshake_stage = 6U;
    }

    g_uart_error = (6U == g_wakeup_stage) ? 0U : 1U;

    return ((0U == g_uart_error) && (1U == g_wifi_ready));
}

static void da16200_connect_set_state (uint8_t state)
{
    g_da16200_connect_state = state;
    g_connect_state_elapsed_ms = 0U;
}

static void da16200_connect_complete (uint8_t result)
{
    g_at_pending = 0U;
    g_async_at_active = false;
    g_scan_active = 0U;
    g_join_active = 0U;
    g_connect_dpm_restore_needed = false;
    g_da16200_connect_result = result;
    da16200_connect_set_state(DA16200_CONNECT_STATE_IDLE);

    if (DA16200_CONNECT_RESULT_OK == result)
    {
        g_wifi_ready = 1U;
        g_uart_error = 0U;
        g_da16200_connect_success_count++;
        g_da16200_wifi_last_offline_ms = g_da16200_wifi_offline_ms;
        g_da16200_wifi_offline_ms = 0U;
    }
    else
    {
        g_wifi_ready = 0U;
        g_uart_error = 1U;
        g_da16200_connect_failure_count++;
    }
}

static void da16200_connect_finish_or_restore (uint8_t result)
{
    g_connect_pending_result = result;
    if (g_connect_dpm_restore_needed)
    {
        da16200_connect_set_state(DA16200_CONNECT_STATE_RESTORE_DPM_SLEEP);
    }
    else
    {
        da16200_connect_complete(result);
    }
}

static void da16200_connect_prepare_scan (void)
{
    g_scan_ap_count = 0U;
    g_scan_first_line[0] = '\0';
    g_scan_error_line[0] = '\0';
    g_target_ap_found = 0U;
    g_target_ap_line[0] = '\0';
    g_scan_active = 1U;
    da16200_connect_set_state(DA16200_CONNECT_STATE_SCAN);
}

static void da16200_connect_start_wakeup (void)
{
    da16200_process_rx();
    g_connect_wakeup_indication_before = g_wakeup_indication_count;
    g_wakeup_wait_ms = 0U;
    g_wakeup_uart_event = 0U;
    g_wakeup_rx_seen = 0U;
    g_connect_dpm_restore_needed = false;

    if (da16200_wakeup_pulse())
    {
        g_wakeup_stage = 4U;
        da16200_connect_set_state(DA16200_CONNECT_STATE_WAIT_WAKE);
    }
    else
    {
        da16200_connect_complete(DA16200_CONNECT_RESULT_WAKE_FAILED);
    }
}

bool DA16200_RequestConnect (void)
{
    fsp_err_t err;

    if ((DA16200_CONNECT_STATE_IDLE != g_da16200_connect_state) ||
        DA16200_MQTT_IsBusy() || g_async_at_active || (0U != g_at_pending))
    {
        return false;
    }

    g_da16200_connect_attempt_count++;
    g_da16200_connect_elapsed_ms = 0U;
    g_da16200_connect_result = DA16200_CONNECT_RESULT_NOT_ATTEMPTED;
    g_connect_pending_result = DA16200_CONNECT_RESULT_NOT_ATTEMPTED;
    g_uart_error = 0U;

    if (!g_uart_opened)
    {
        g_rx_head = 0U;
        g_rx_tail = 0U;
        g_rx_received = 0U;
        g_rx_dropped = 0U;
        g_rx_high_watermark = 0U;
        g_rx_lines = 0U;
        g_rx_unsolicited_lines = 0U;
        g_rx_line_overflows = 0U;
        g_rx_invalid_bytes = 0U;
        g_rx_partial_resets = 0U;
        g_uart_error_events = 0U;
        g_wakeup_pulse_count = 0U;
        g_wakeup_indication_count = 0U;
        g_at_attempt = 0U;
        g_da16200_mqtt_config_result = 0U;
        g_mqtt_config_applied = false;
        g_da16200_mqtt_service_state = DA16200_MQTT_STATE_IDLE;
        g_da16200_mqtt_service_elapsed_ms = 0U;
        g_mqtt_result_ready = false;

        err = g_uart0.p_api->open(g_uart0.p_ctrl, g_uart0.p_cfg);
        if (FSP_SUCCESS != err)
        {
            da16200_connect_complete(DA16200_CONNECT_RESULT_UART_OPEN_FAILED);
            return true;
        }
        g_uart_opened = true;
        da16200_connect_set_state(DA16200_CONNECT_STATE_POWER_ON_WAIT);
        return true;
    }

    da16200_connect_start_wakeup();
    return true;
}

bool DA16200_ConnectIsBusy (void)
{
    return (DA16200_CONNECT_STATE_IDLE != g_da16200_connect_state);
}

void DA16200_ConnectService (void)
{
    static char const mcu_wakeup_done_command[] = "AT+MCUWUDONE\r\n";
    static char const clear_dpm_sleep_command[] = "AT+CLRDPMSLPEXT\r\n";
    static char const restore_dpm_sleep_command[] = "AT+SETDPMSLPEXT\r\n";
    static char const sync_command[] = "AT\r\n";
    static char const wifi_query_command[] = "AT+WFSTA\r\n";
    static char const scan_command[] = "AT+WFSCAN\r\n";
    static char const join_command[] = "AT+WFJAPA=" WIFI_SSID "," WIFI_PASSWORD "\r\n";
    uint8_t command_result;

    if (DA16200_CONNECT_STATE_IDLE == g_da16200_connect_state)
    {
        return;
    }

    g_da16200_connect_elapsed_ms++;
    g_connect_state_elapsed_ms++;

    switch (g_da16200_connect_state)
    {
        case DA16200_CONNECT_STATE_POWER_ON_WAIT:
            da16200_process_rx();
            if (g_connect_state_elapsed_ms >= DA16200_POWER_ON_DELAY_MS)
            {
                da16200_connect_start_wakeup();
            }
            break;

        case DA16200_CONNECT_STATE_WAIT_WAKE:
            da16200_process_rx();
            g_wakeup_wait_ms = g_connect_state_elapsed_ms;
            if (g_wakeup_indication_count != g_connect_wakeup_indication_before)
            {
                g_wakeup_rx_seen = 1U;
                g_wakeup_uart_event = g_last_uart_event;
                g_last_uart_event = 0U;
                g_dpm_handshake_stage = 1U;
                da16200_connect_set_state(DA16200_CONNECT_STATE_MCU_WAKE_DONE);
            }
            else if (g_connect_state_elapsed_ms >= DA16200_WAKE_RX_MAX_WAIT_MS)
            {
                g_wakeup_uart_event = g_last_uart_event;
                g_last_uart_event = 0U;
                da16200_connect_set_state(DA16200_CONNECT_STATE_SYNC);
            }
            break;

        case DA16200_CONNECT_STATE_MCU_WAKE_DONE:
            command_result = da16200_async_command_step(mcu_wakeup_done_command,
                                                         NULL,
                                                         NULL,
                                                         0U,
                                                         DA16200_RESPONSE_TIMEOUT_MS);
            if (1U == command_result)
            {
                g_mcuwudone_result = 1U;
                g_dpm_handshake_stage = 2U;
                da16200_connect_set_state(DA16200_CONNECT_STATE_CLEAR_DPM_SLEEP);
            }
            else if (0U != command_result)
            {
                g_mcuwudone_result = command_result;
                da16200_connect_complete(DA16200_CONNECT_RESULT_DPM_HANDSHAKE_FAILED);
            }
            break;

        case DA16200_CONNECT_STATE_CLEAR_DPM_SLEEP:
            command_result = da16200_async_command_step(clear_dpm_sleep_command,
                                                         NULL,
                                                         NULL,
                                                         0U,
                                                         DA16200_RESPONSE_TIMEOUT_MS);
            if (1U == command_result)
            {
                g_clear_dpm_sleep_result = 1U;
                g_connect_dpm_restore_needed = true;
                g_dpm_handshake_stage = 3U;
                da16200_connect_set_state(DA16200_CONNECT_STATE_SYNC);
            }
            else if (0U != command_result)
            {
                g_clear_dpm_sleep_result = command_result;
                da16200_connect_complete(DA16200_CONNECT_RESULT_DPM_HANDSHAKE_FAILED);
            }
            break;

        case DA16200_CONNECT_STATE_SYNC:
            command_result = da16200_async_command_step(sync_command,
                                                         NULL,
                                                         NULL,
                                                         0U,
                                                         DA16200_RESPONSE_TIMEOUT_MS);
            if (1U == command_result)
            {
                g_at_sync_result = 1U;
                g_wifi_status = 0U;
                da16200_connect_set_state(DA16200_CONNECT_STATE_WIFI_QUERY);
            }
            else if (0U != command_result)
            {
                g_at_sync_result = command_result;
                da16200_connect_finish_or_restore(DA16200_CONNECT_RESULT_SYNC_FAILED);
            }
            break;

        case DA16200_CONNECT_STATE_WIFI_QUERY:
            command_result = da16200_async_command_step(wifi_query_command,
                                                         "+WFSTA:",
                                                         g_wifi_status_line,
                                                         sizeof(g_wifi_status_line),
                                                         DA16200_RESPONSE_TIMEOUT_MS);
            if (1U == command_result)
            {
                g_wifi_status_result = 1U;
                if (0 == strcmp(g_wifi_status_line, "+WFSTA:1"))
                {
                    g_wifi_status = 1U;
                    da16200_connect_finish_or_restore(DA16200_CONNECT_RESULT_OK);
                }
                else
                {
                    g_wifi_status = 2U;
                    g_wifi_ready = 0U;
                    da16200_connect_prepare_scan();
                }
            }
            else if (0U != command_result)
            {
                g_wifi_status_result = command_result;
                g_wifi_ready = 0U;
                da16200_connect_prepare_scan();
            }
            break;

        case DA16200_CONNECT_STATE_SCAN:
            command_result = da16200_async_command_step(scan_command,
                                                         "+WFSCAN:",
                                                         g_scan_first_line,
                                                         sizeof(g_scan_first_line),
                                                         DA16200_SCAN_TIMEOUT_MS);
            if (0U != command_result)
            {
                g_scan_active = 0U;
                g_scan_result = command_result;
                if (1U != command_result)
                {
                    da16200_connect_finish_or_restore(DA16200_CONNECT_RESULT_SCAN_FAILED);
                }
                else if (0U == g_target_ap_found)
                {
                    g_join_result = 4U;
                    da16200_connect_finish_or_restore(DA16200_CONNECT_RESULT_AP_NOT_FOUND);
                }
                else
                {
                    g_join_command_result = 0U;
                    g_join_result = 0U;
                    g_join_line[0] = '\0';
                    g_join_active = 1U;
                    da16200_connect_set_state(DA16200_CONNECT_STATE_JOIN_COMMAND);
                }
            }
            break;

        case DA16200_CONNECT_STATE_JOIN_COMMAND:
            command_result = da16200_async_command_step(join_command,
                                                         NULL,
                                                         NULL,
                                                         0U,
                                                         DA16200_RESPONSE_TIMEOUT_MS);
            if (1U == command_result)
            {
                g_join_command_result = 1U;
                if (1U == g_join_result)
                {
                    g_join_active = 0U;
                    da16200_connect_finish_or_restore(DA16200_CONNECT_RESULT_OK);
                }
                else
                {
                    da16200_connect_set_state(DA16200_CONNECT_STATE_JOIN_WAIT);
                }
            }
            else if (0U != command_result)
            {
                g_join_command_result = command_result;
                g_join_active = 0U;
                da16200_connect_finish_or_restore(DA16200_CONNECT_RESULT_JOIN_COMMAND_FAILED);
            }
            break;

        case DA16200_CONNECT_STATE_JOIN_WAIT:
            da16200_process_rx();
            if (1U == g_join_result)
            {
                g_join_active = 0U;
                da16200_connect_finish_or_restore(DA16200_CONNECT_RESULT_OK);
            }
            else if (0U != g_join_result)
            {
                g_join_active = 0U;
                da16200_connect_finish_or_restore(DA16200_CONNECT_RESULT_JOIN_FAILED);
            }
            else if (g_connect_state_elapsed_ms >= DA16200_JOIN_TIMEOUT_MS)
            {
                g_join_result = 3U;
                g_join_active = 0U;
                da16200_connect_finish_or_restore(DA16200_CONNECT_RESULT_JOIN_FAILED);
            }
            break;

        case DA16200_CONNECT_STATE_RESTORE_DPM_SLEEP:
            g_dpm_handshake_stage = 4U;
            command_result = da16200_async_command_step(restore_dpm_sleep_command,
                                                         NULL,
                                                         NULL,
                                                         0U,
                                                         DA16200_RESPONSE_TIMEOUT_MS);
            if (1U == command_result)
            {
                g_set_dpm_sleep_result = 1U;
                g_dpm_handshake_stage = 5U;
                da16200_connect_complete(g_connect_pending_result);
            }
            else if (0U != command_result)
            {
                g_set_dpm_sleep_result = command_result;
                g_dpm_handshake_stage = 6U;
                da16200_connect_complete((DA16200_CONNECT_RESULT_OK == g_connect_pending_result) ?
                                         DA16200_CONNECT_RESULT_DPM_RESTORE_FAILED :
                                         g_connect_pending_result);
            }
            break;

        default:
            da16200_connect_complete(DA16200_CONNECT_RESULT_SYNC_FAILED);
            break;
    }
}

void DA16200_ServiceDelay (uint32_t delay_ms)
{
    while (delay_ms-- > 0U)
    {
        DA16200_ConnectService();
        DA16200_MQTT_Service();
        da16200_service_delay(1U);
        if (0U == g_wifi_ready)
        {
            if (g_da16200_wifi_offline_ms < UINT32_MAX)
            {
                g_da16200_wifi_offline_ms++;
            }
        }
    }
}

bool DA16200_IsReady (void)
{
    return ((0U == g_uart_error) && (1U == g_wifi_ready));
}

static void da16200_mqtt_set_state (uint8_t state)
{
    g_da16200_mqtt_service_state = state;
    g_mqtt_state_elapsed_ms = 0U;
}

static void da16200_mqtt_complete (uint8_t result)
{
    g_at_pending = 0U;
    g_async_at_active = false;
    g_mqtt_dpm_session_active = false;
    g_mqtt_pending_result = result;
    g_da16200_mqtt_publish_result = result;
    g_mqtt_result_ready = true;
    da16200_mqtt_set_state(DA16200_MQTT_STATE_IDLE);

    if (DA16200_MQTT_PUBLISH_OK == result)
    {
        g_da16200_mqtt_publish_success_count++;
    }
    else
    {
        g_da16200_mqtt_publish_failure_count++;
    }
}

static void da16200_mqtt_finish_or_restore (uint8_t result)
{
    g_mqtt_pending_result = result;
    if (g_mqtt_dpm_session_active)
    {
        da16200_mqtt_set_state(DA16200_MQTT_STATE_RESTORE_DPM_SLEEP);
    }
    else
    {
        da16200_mqtt_complete(result);
    }
}

static void da16200_mqtt_config_failed (uint8_t config_result)
{
    g_da16200_mqtt_config_result = config_result;
    g_mqtt_config_applied = false;
    da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_CONFIG_FAILED);
}

static bool da16200_mqtt_format_control_command (char const * p_format,
                                                  char const * p_text,
                                                  uint32_t number)
{
    int length = snprintf(g_mqtt_control_command,
                          sizeof(g_mqtt_control_command),
                          p_format,
                          p_text,
                          (unsigned long) number);

    return ((length > 0) && ((size_t) length < sizeof(g_mqtt_control_command)));
}

static void da16200_mqtt_begin_publish_command (void)
{
    g_mqtt_publish_event_before = g_da16200_mqtt_publish_event_count;
    g_mqtt_publish_event_result = DA16200_MQTT_PUBLISH_NOT_ATTEMPTED;
    da16200_mqtt_set_state(DA16200_MQTT_STATE_PUBLISH_COMMAND);
}

bool DA16200_MQTT_RequestPublish (char const * p_payload)
{
    size_t payload_length;
    int command_length;

    if ((DA16200_MQTT_STATE_IDLE != g_da16200_mqtt_service_state) ||
        DA16200_ConnectIsBusy() || g_mqtt_result_ready ||
        g_async_at_active || (0U != g_at_pending))
    {
        return false;
    }

    g_da16200_mqtt_publish_attempt_count++;
    g_da16200_mqtt_publish_error = 0;
    g_da16200_mqtt_service_elapsed_ms = 0U;
    g_mqtt_pending_result = DA16200_MQTT_PUBLISH_NOT_ATTEMPTED;
    g_da16200_mqtt_publish_result = DA16200_MQTT_PUBLISH_NOT_ATTEMPTED;

    if (NULL == p_payload)
    {
        da16200_mqtt_complete(DA16200_MQTT_PUBLISH_INVALID_PAYLOAD);
        return true;
    }

    payload_length = strlen(p_payload);
    if ((0U == payload_length) ||
        (NULL != strchr(p_payload, '\'')) ||
        (NULL != strchr(p_payload, '\r')) ||
        (NULL != strchr(p_payload, '\n')))
    {
        da16200_mqtt_complete(DA16200_MQTT_PUBLISH_INVALID_PAYLOAD);
        return true;
    }

    command_length = snprintf(g_mqtt_publish_command,
                              sizeof(g_mqtt_publish_command),
                              "AT+NWMQMSG='%s'\r\n",
                              p_payload);
    if ((command_length <= 0) ||
        ((size_t) command_length >= sizeof(g_mqtt_publish_command)))
    {
        da16200_mqtt_complete(DA16200_MQTT_PUBLISH_INVALID_PAYLOAD);
        return true;
    }

    if (!g_uart_opened)
    {
        da16200_mqtt_complete(DA16200_MQTT_PUBLISH_NOT_READY);
        return true;
    }

    if (!da16200_mqtt_field_valid(DA16200_MQTT_BROKER_HOST) ||
        !da16200_mqtt_field_valid(BEMFA_MQTT_TOPIC) ||
        !da16200_mqtt_field_valid(BEMFA_MQTT_PRIVATE_KEY) ||
        (0U == DA16200_MQTT_BROKER_PORT))
    {
        g_da16200_mqtt_config_result = 2U;
        da16200_mqtt_complete(DA16200_MQTT_PUBLISH_CONFIG_INVALID);
        return true;
    }

    da16200_process_rx();
    g_mqtt_wakeup_indication_before = g_wakeup_indication_count;
    if (!da16200_wakeup_pulse())
    {
        da16200_mqtt_complete(DA16200_MQTT_PUBLISH_SESSION_FAILED);
        return true;
    }

    g_mqtt_dpm_session_active = false;
    da16200_mqtt_set_state(DA16200_MQTT_STATE_WAIT_WAKE);
    return true;
}

bool DA16200_MQTT_IsBusy (void)
{
    return (DA16200_MQTT_STATE_IDLE != g_da16200_mqtt_service_state);
}

bool DA16200_MQTT_TakeResult (uint8_t * p_result)
{
    if (!g_mqtt_result_ready || (NULL == p_result))
    {
        return false;
    }

    *p_result = g_mqtt_pending_result;
    g_mqtt_result_ready = false;
    return true;
}

void DA16200_MQTT_Service (void)
{
    static char const mcu_wakeup_done_command[] = "AT+MCUWUDONE\r\n";
    static char const clear_dpm_sleep_command[] = "AT+CLRDPMSLPEXT\r\n";
    static char const restore_dpm_sleep_command[] = "AT+SETDPMSLPEXT\r\n";
    static char const sync_command[] = "AT\r\n";
    static char const wifi_query_command[] = "AT+WFSTA\r\n";
    static char const mqtt_stop_command[] = "AT+NWMQCL=0\r\n";
    static char const mqtt_qos_command[] = "AT+NWMQQOS=0\r\n";
    static char const mqtt_auto_command[] = "AT+NWMQAUTO=1\r\n";
    static char const mqtt_query_command[] = "AT+NWMQCL\r\n";
    static char const mqtt_start_command[] = "AT+NWMQCL=1\r\n";
    uint8_t command_result;

    if (DA16200_MQTT_STATE_IDLE == g_da16200_mqtt_service_state)
    {
        da16200_process_rx();
        return;
    }

    g_da16200_mqtt_service_elapsed_ms++;
    g_mqtt_state_elapsed_ms++;

    switch (g_da16200_mqtt_service_state)
    {
        case DA16200_MQTT_STATE_WAIT_WAKE:
            da16200_process_rx();
            if (g_wakeup_indication_count != g_mqtt_wakeup_indication_before)
            {
                g_mqtt_dpm_session_active = true;
                da16200_mqtt_set_state(DA16200_MQTT_STATE_MCU_WAKE_DONE);
            }
            else if (g_mqtt_state_elapsed_ms >= DA16200_WAKE_RX_MAX_WAIT_MS)
            {
                da16200_mqtt_set_state(DA16200_MQTT_STATE_SYNC);
            }
            break;

        case DA16200_MQTT_STATE_MCU_WAKE_DONE:
            command_result = da16200_mqtt_command_step(mcu_wakeup_done_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                g_mcuwudone_result = 1U;
                da16200_mqtt_set_state(DA16200_MQTT_STATE_CLEAR_DPM_SLEEP);
            }
            else if (0U != command_result)
            {
                g_mcuwudone_result = command_result;
                da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_SESSION_FAILED);
            }
            break;

        case DA16200_MQTT_STATE_CLEAR_DPM_SLEEP:
            command_result = da16200_mqtt_command_step(clear_dpm_sleep_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                g_clear_dpm_sleep_result = 1U;
                da16200_mqtt_set_state(DA16200_MQTT_STATE_SYNC);
            }
            else if (0U != command_result)
            {
                g_clear_dpm_sleep_result = command_result;
                da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_SESSION_FAILED);
            }
            break;

        case DA16200_MQTT_STATE_SYNC:
            command_result = da16200_mqtt_command_step(sync_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                g_at_sync_result = 1U;
                g_wifi_status = 0U;
                da16200_mqtt_set_state(DA16200_MQTT_STATE_WIFI_QUERY);
            }
            else if (0U != command_result)
            {
                g_at_sync_result = command_result;
                da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_SESSION_FAILED);
            }
            break;

        case DA16200_MQTT_STATE_WIFI_QUERY:
            command_result = da16200_mqtt_command_step(wifi_query_command,
                                                        "+WFSTA:",
                                                        g_wifi_status_line,
                                                        sizeof(g_wifi_status_line));
            if (1U == command_result)
            {
                g_wifi_status_result = 1U;
                if (0 == strcmp(g_wifi_status_line, "+WFSTA:1"))
                {
                    g_wifi_status = 1U;
                    g_wifi_ready = 1U;
                    da16200_mqtt_set_state(g_mqtt_config_applied ?
                                           DA16200_MQTT_STATE_MQTT_QUERY :
                                           DA16200_MQTT_STATE_CONFIG_STOP);
                }
                else
                {
                    g_wifi_status = 2U;
                    g_wifi_ready = 0U;
                    da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_WIFI_DISCONNECTED);
                }
            }
            else if (0U != command_result)
            {
                g_wifi_status_result = command_result;
                g_wifi_ready = 0U;
                da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_WIFI_DISCONNECTED);
            }
            break;

        case DA16200_MQTT_STATE_CONFIG_STOP:
            g_mqtt_config_applied = false;
            command_result = da16200_mqtt_command_step(mqtt_stop_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                da16200_mqtt_set_state(DA16200_MQTT_STATE_CONFIG_BROKER);
            }
            else if (0U != command_result)
            {
                da16200_mqtt_config_failed(3U);
            }
            break;

        case DA16200_MQTT_STATE_CONFIG_BROKER:
            if (!da16200_mqtt_format_control_command("AT+NWMQBR=%s,%lu\r\n",
                                                       DA16200_MQTT_BROKER_HOST,
                                                       DA16200_MQTT_BROKER_PORT))
            {
                da16200_mqtt_config_failed(4U);
                break;
            }
            command_result = da16200_mqtt_command_step(g_mqtt_control_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                da16200_mqtt_set_state(DA16200_MQTT_STATE_CONFIG_SUB_TOPIC);
            }
            else if (0U != command_result)
            {
                da16200_mqtt_config_failed(4U);
            }
            break;

        case DA16200_MQTT_STATE_CONFIG_SUB_TOPIC:
            if (!da16200_mqtt_format_control_command("AT+NWMQTS=1,%s\r\n",
                                                       BEMFA_MQTT_TOPIC,
                                                       0U))
            {
                da16200_mqtt_config_failed(5U);
                break;
            }
            command_result = da16200_mqtt_command_step(g_mqtt_control_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                da16200_mqtt_set_state(DA16200_MQTT_STATE_CONFIG_PUB_TOPIC);
            }
            else if (0U != command_result)
            {
                da16200_mqtt_config_failed(5U);
            }
            break;

        case DA16200_MQTT_STATE_CONFIG_PUB_TOPIC:
            if (!da16200_mqtt_format_control_command("AT+NWMQTP=%s\r\n",
                                                       BEMFA_MQTT_TOPIC,
                                                       0U))
            {
                da16200_mqtt_config_failed(6U);
                break;
            }
            command_result = da16200_mqtt_command_step(g_mqtt_control_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                da16200_mqtt_set_state(DA16200_MQTT_STATE_CONFIG_CLIENT_ID);
            }
            else if (0U != command_result)
            {
                da16200_mqtt_config_failed(6U);
            }
            break;

        case DA16200_MQTT_STATE_CONFIG_CLIENT_ID:
            if (!da16200_mqtt_format_control_command("AT+NWMQCID=%s\r\n",
                                                       BEMFA_MQTT_PRIVATE_KEY,
                                                       0U))
            {
                da16200_mqtt_config_failed(7U);
                break;
            }
            command_result = da16200_mqtt_command_step(g_mqtt_control_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                da16200_mqtt_set_state(DA16200_MQTT_STATE_CONFIG_QOS);
            }
            else if (0U != command_result)
            {
                da16200_mqtt_config_failed(7U);
            }
            break;

        case DA16200_MQTT_STATE_CONFIG_QOS:
            command_result = da16200_mqtt_command_step(mqtt_qos_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                da16200_mqtt_set_state(DA16200_MQTT_STATE_CONFIG_AUTO);
            }
            else if (0U != command_result)
            {
                da16200_mqtt_config_failed(8U);
            }
            break;

        case DA16200_MQTT_STATE_CONFIG_AUTO:
            command_result = da16200_mqtt_command_step(mqtt_auto_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                g_da16200_mqtt_config_result = 1U;
                g_mqtt_config_applied = true;
                da16200_mqtt_set_state(DA16200_MQTT_STATE_MQTT_QUERY);
            }
            else if (0U != command_result)
            {
                da16200_mqtt_config_failed(9U);
            }
            break;

        case DA16200_MQTT_STATE_MQTT_QUERY:
            command_result = da16200_mqtt_command_step(mqtt_query_command,
                                                        "+NWMQCL:",
                                                        g_mqtt_status_line,
                                                        sizeof(g_mqtt_status_line));
            if (1U == command_result)
            {
                g_da16200_mqtt_query_result = 1U;
                if (1U == g_da16200_mqtt_client_state)
                {
                    g_da16200_mqtt_start_result = 1U;
                    da16200_mqtt_begin_publish_command();
                }
                else
                {
                    g_mqtt_client_event_before = g_da16200_mqtt_client_event_count;
                    if (0U == g_da16200_mqtt_client_state)
                    {
                        da16200_mqtt_set_state(DA16200_MQTT_STATE_MQTT_START);
                    }
                    else
                    {
                        da16200_mqtt_set_state(DA16200_MQTT_STATE_MQTT_CONNECT_WAIT);
                    }
                }
            }
            else if (0U != command_result)
            {
                g_da16200_mqtt_query_result = command_result;
                da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_MQTT_DISCONNECTED);
            }
            break;

        case DA16200_MQTT_STATE_MQTT_START:
            command_result = da16200_mqtt_command_step(mqtt_start_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                if (1U == g_da16200_mqtt_client_state)
                {
                    g_da16200_mqtt_start_result = 1U;
                    da16200_mqtt_begin_publish_command();
                }
                else
                {
                    da16200_mqtt_set_state(DA16200_MQTT_STATE_MQTT_CONNECT_WAIT);
                }
            }
            else if (0U != command_result)
            {
                g_da16200_mqtt_start_result = 4U;
                da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_MQTT_DISCONNECTED);
            }
            break;

        case DA16200_MQTT_STATE_MQTT_CONNECT_WAIT:
            da16200_process_rx();
            if (1U == g_da16200_mqtt_client_state)
            {
                g_da16200_mqtt_start_result = 1U;
                da16200_mqtt_begin_publish_command();
            }
            else if ((g_da16200_mqtt_client_event_count != g_mqtt_client_event_before) &&
                     (0U == g_da16200_mqtt_client_state))
            {
                g_da16200_mqtt_start_result = 2U;
                da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_MQTT_DISCONNECTED);
            }
            else if (g_mqtt_state_elapsed_ms >= DA16200_MQTT_CONNECT_TIMEOUT_MS)
            {
                g_da16200_mqtt_start_result = 3U;
                da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_MQTT_DISCONNECTED);
            }
            break;

        case DA16200_MQTT_STATE_PUBLISH_COMMAND:
            command_result = da16200_mqtt_command_step(g_mqtt_publish_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                da16200_mqtt_set_state(DA16200_MQTT_STATE_PUBLISH_ACK_WAIT);
            }
            else if (0U != command_result)
            {
                da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_COMMAND_FAILED);
            }
            break;

        case DA16200_MQTT_STATE_PUBLISH_ACK_WAIT:
            da16200_process_rx();
            if (g_da16200_mqtt_publish_event_count != g_mqtt_publish_event_before)
            {
                da16200_mqtt_finish_or_restore(g_mqtt_publish_event_result);
            }
            else if (g_mqtt_state_elapsed_ms >= DA16200_MQTT_PUBLISH_TIMEOUT_MS)
            {
                da16200_mqtt_finish_or_restore(DA16200_MQTT_PUBLISH_ACK_TIMEOUT);
            }
            break;

        case DA16200_MQTT_STATE_RESTORE_DPM_SLEEP:
            command_result = da16200_mqtt_command_step(restore_dpm_sleep_command, NULL, NULL, 0U);
            if (1U == command_result)
            {
                g_set_dpm_sleep_result = 1U;
                da16200_mqtt_complete(g_mqtt_pending_result);
            }
            else if (0U != command_result)
            {
                g_set_dpm_sleep_result = command_result;
                da16200_mqtt_complete((DA16200_MQTT_PUBLISH_OK == g_mqtt_pending_result) ?
                                      DA16200_MQTT_PUBLISH_DPM_RESTORE_FAILED :
                                      g_mqtt_pending_result);
            }
            break;

        default:
            da16200_mqtt_complete(DA16200_MQTT_PUBLISH_SESSION_FAILED);
            break;
    }
}

uint8_t DA16200_MQTT_Publish (char const * p_payload)
{
    uint32_t wakeup_indication_before;
    uint32_t publish_event_before;
    uint8_t result = DA16200_MQTT_PUBLISH_NOT_ATTEMPTED;
    bool dpm_session_active = false;
    size_t payload_length;
    int command_length;

    g_da16200_mqtt_publish_attempt_count++;
    g_da16200_mqtt_publish_error = 0;

    if (NULL == p_payload)
    {
        result = DA16200_MQTT_PUBLISH_INVALID_PAYLOAD;
        goto publish_complete;
    }

    payload_length = strlen(p_payload);
    if ((0U == payload_length) ||
        (NULL != strchr(p_payload, '\'')) ||
        (NULL != strchr(p_payload, '\r')) ||
        (NULL != strchr(p_payload, '\n')))
    {
        result = DA16200_MQTT_PUBLISH_INVALID_PAYLOAD;
        goto publish_complete;
    }

    command_length = snprintf(g_mqtt_publish_command,
                              sizeof(g_mqtt_publish_command),
                              "AT+NWMQMSG='%s'\r\n",
                              p_payload);
    if ((command_length <= 0) ||
        ((size_t) command_length >= sizeof(g_mqtt_publish_command)))
    {
        result = DA16200_MQTT_PUBLISH_INVALID_PAYLOAD;
        goto publish_complete;
    }

    if (!g_uart_opened)
    {
        result = DA16200_MQTT_PUBLISH_NOT_READY;
        goto publish_complete;
    }

    da16200_process_rx();
    wakeup_indication_before = g_wakeup_indication_count;
    if (!da16200_wakeup_pulse())
    {
        result = DA16200_MQTT_PUBLISH_SESSION_FAILED;
        goto publish_complete;
    }

    for (uint32_t elapsed = 0U; elapsed < DA16200_WAKE_RX_MAX_WAIT_MS; elapsed++)
    {
        da16200_process_rx();
        if (g_wakeup_indication_count != wakeup_indication_before)
        {
            dpm_session_active = true;
            break;
        }
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    if (dpm_session_active)
    {
        if ((1U != da16200_notify_mcu_wakeup_done()) ||
            (1U != da16200_clear_dpm_sleep_ext()))
        {
            result = DA16200_MQTT_PUBLISH_SESSION_FAILED;
            goto restore_dpm;
        }
    }

    if (1U != da16200_sync())
    {
        result = DA16200_MQTT_PUBLISH_SESSION_FAILED;
        goto restore_dpm;
    }

    if ((1U != da16200_query_wifi_status()) || (1U != g_wifi_status))
    {
        g_wifi_ready = 0U;
        result = DA16200_MQTT_PUBLISH_WIFI_DISCONNECTED;
        goto restore_dpm;
    }
    g_wifi_ready = 1U;

    if (!g_mqtt_config_applied)
    {
        g_da16200_mqtt_config_result = da16200_apply_mqtt_config();
        if (1U != g_da16200_mqtt_config_result)
        {
            result = (2U == g_da16200_mqtt_config_result) ?
                     DA16200_MQTT_PUBLISH_CONFIG_INVALID :
                     DA16200_MQTT_PUBLISH_CONFIG_FAILED;
            goto restore_dpm;
        }
    }

    if (1U != da16200_query_mqtt_status())
    {
        result = DA16200_MQTT_PUBLISH_MQTT_DISCONNECTED;
        goto restore_dpm;
    }

    if (0U == g_da16200_mqtt_client_state)
    {
        g_da16200_mqtt_start_result = da16200_start_mqtt_client();
    }
    else if (2U == g_da16200_mqtt_client_state)
    {
        g_da16200_mqtt_start_result = da16200_wait_for_mqtt_connection();
    }
    else
    {
        g_da16200_mqtt_start_result = 1U;
    }

    if ((1U != g_da16200_mqtt_start_result) ||
        (1U != g_da16200_mqtt_client_state))
    {
        result = DA16200_MQTT_PUBLISH_MQTT_DISCONNECTED;
        goto restore_dpm;
    }

    publish_event_before = g_da16200_mqtt_publish_event_count;
    g_mqtt_publish_event_result = DA16200_MQTT_PUBLISH_NOT_ATTEMPTED;
    if (1U != da16200_execute_command(g_mqtt_publish_command,
                                      NULL,
                                      NULL,
                                      0U,
                                      DA16200_RESPONSE_TIMEOUT_MS))
    {
        result = DA16200_MQTT_PUBLISH_COMMAND_FAILED;
        goto restore_dpm;
    }

    for (uint32_t elapsed = 0U;
         (elapsed < DA16200_MQTT_PUBLISH_TIMEOUT_MS) &&
         (g_da16200_mqtt_publish_event_count == publish_event_before);
         elapsed++)
    {
        da16200_process_rx();
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    if (g_da16200_mqtt_publish_event_count == publish_event_before)
    {
        result = DA16200_MQTT_PUBLISH_ACK_TIMEOUT;
    }
    else
    {
        result = g_mqtt_publish_event_result;
    }

restore_dpm:
    if (dpm_session_active && (1U != da16200_set_dpm_sleep_ext()))
    {
        if (DA16200_MQTT_PUBLISH_OK == result)
        {
            result = DA16200_MQTT_PUBLISH_DPM_RESTORE_FAILED;
        }
    }

publish_complete:
    g_da16200_mqtt_publish_result = result;
    if (DA16200_MQTT_PUBLISH_OK == result)
    {
        g_da16200_mqtt_publish_success_count++;
    }
    else
    {
        g_da16200_mqtt_publish_failure_count++;
    }

    return result;
}
