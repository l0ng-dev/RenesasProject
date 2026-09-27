#include "hal_data.h"
#include "replay_dataset.h"
#include "stage2_config.h"
#include "wifi_credentials.h"
#include <stdio.h>
#include <string.h>
#if (1 == BSP_MULTICORE_PROJECT) && BSP_TZ_SECURE_BUILD
bsp_ipc_semaphore_handle_t g_core_start_semaphore =
{
    .semaphore_num = 0
};
#endif
#define DA16200_RX_BUFFER_SIZE       (256U)
#define DA16200_LINE_SIZE            (192U)
#define DA16200_RESPONSE_TIMEOUT_MS  (1500U)
#define DA16200_SCAN_TIMEOUT_MS      (15000U)
#define DA16200_JOIN_TIMEOUT_MS      (45000U)
#define DA16200_RETRY_INTERVAL_MS    (2000U)
#define DA16200_WAKE_PULSE_MS        (1U)
#define DA16200_WAKE_RX_MAX_WAIT_MS  (1500U)
#define DA16200_MQTT_CONNECT_TIMEOUT_MS (15000U)
#define DA16200_MQTT_PUBLISH_TIMEOUT_MS (15000U)
#define DA16200_MQTT_COMMAND_SIZE       (256U)
#define DA16200_WAKE_INDICATION_SUFFIX "WAKEUP,EXT"
/* Retain active Wi-Fi/configuration helpers for future diagnostics, but keep
 * them out of the read-only UART bring-up firmware. */
#define DA16200_ENABLE_ACTIVE_CONTROL (0U)
/* Retain detailed read-only queries for the next stage. The current stage
 * validates only RTC_WAKE_UP followed by a basic AT/OK exchange. */
#define DA16200_ENABLE_READ_ONLY_DETAILS (0U)

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
static volatile uint8_t g_version_result;
static volatile uint8_t g_wifi_mode_result;
static volatile uint8_t g_country_result;
static volatile uint8_t g_country_set_result;
static volatile uint8_t g_dpm_result;
static volatile uint8_t g_dpm_set_result;
static volatile uint8_t g_cancel_ap_result;
static volatile uint32_t g_startup_cancel_attempts;
static volatile uint8_t g_scan_result;
static volatile uint8_t g_scan_active;
static volatile uint8_t g_join_command_result;
static volatile uint8_t g_join_result;
static volatile uint8_t g_join_active;
static volatile uint8_t g_target_ap_found;
static volatile uint32_t g_scan_ap_count;
/* Stage 2: 0 disabled/not run, 1 complete, 2 local config missing,
 * 3 DA16200 MQTT config failed, 4 broker connection failed,
 * 5 replay publish failed. */
static volatile uint8_t g_stage2_result;
static volatile uint32_t g_stage2_sample_index;
static volatile uint32_t g_replay_publish_attempts;
static volatile uint32_t g_replay_publish_successes;
static volatile uint32_t g_replay_publish_failures;
static volatile uint8_t g_mqtt_client_state;
static volatile uint8_t g_mqtt_publish_result;
static volatile uint32_t g_mqtt_client_event_count;
static volatile uint32_t g_mqtt_publish_event_count;
static char g_version_line[DA16200_LINE_SIZE];
static char g_wifi_mode_line[DA16200_LINE_SIZE];
static char g_country_line[DA16200_LINE_SIZE];
static char g_dpm_line[DA16200_LINE_SIZE];
static char g_scan_first_line[DA16200_LINE_SIZE];
static volatile char g_target_ap_line[DA16200_LINE_SIZE];
static volatile char g_join_line[DA16200_LINE_SIZE];
/* Preserve a scan ERROR line even if a later unsolicited event arrives. */
static volatile char g_scan_error_line[DA16200_LINE_SIZE];
static volatile char g_mqtt_client_line[DA16200_LINE_SIZE];
static volatile char g_mqtt_publish_line[DA16200_LINE_SIZE];
/* Volatile keeps the latest complete response line visible in Keil Watch. */
static volatile char g_last_rx_line[DA16200_LINE_SIZE];
static char g_rx_line[DA16200_LINE_SIZE];
static uint32_t g_rx_line_length;
static bool g_rx_line_discarding;
static char const * gp_at_expected_prefix;
static char * gp_at_response_line;
static size_t g_at_response_line_size;

static bool da16200_is_sensitive_command_echo (char const * p_line)
{
    return ((0 == strncmp(p_line, "AT+WFJAPA=", 10U)) ||
            (0 == strncmp(p_line, "AT+NWMQCID=", 11U)) ||
            (0 == strncmp(p_line, "AT+NWMQLI=", 10U)) ||
            (0 == strncmp(p_line, "AT+NWMQTT=", 10U)));
}

static bool da16200_is_hex_digit (char value)
{
    return (((value >= '0') && (value <= '9')) ||
            ((value >= 'A') && (value <= 'F')) ||
            ((value >= 'a') && (value <= 'f')));
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
    size_t length = strlen(p_line);
    char const * p_scan_line = p_line;

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

    if (0 == strncmp(p_line, "+NWMQCL:", 8U))
    {
        for (size_t index = 0U; index <= length; index++)
        {
            g_mqtt_client_line[index] = p_line[index];
        }

        if ((9U == length) && ('0' <= p_line[8]) && ('2' >= p_line[8]))
        {
            g_mqtt_client_state = (uint8_t) (p_line[8] - '0');
        }
        g_mqtt_client_event_count++;
    }

    if (0 == strncmp(p_line, "+NWMQMSGSND:", 12U))
    {
        for (size_t index = 0U; index <= length; index++)
        {
            g_mqtt_publish_line[index] = p_line[index];
        }

        g_mqtt_publish_result = (0 == strncmp(p_line, "+NWMQMSGSND:1", 13U)) ? 1U : 2U;
        g_mqtt_publish_event_count++;
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

#if DA16200_ENABLE_READ_ONLY_DETAILS
static uint8_t da16200_query_version (void)
{
    static char const command[] = "AT+VER\r\n";

    return da16200_execute_command(command,
                                   "+VER:",
                                   g_version_line,
                                   sizeof(g_version_line),
                                   DA16200_RESPONSE_TIMEOUT_MS);
}
#endif

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

static bool da16200_stage2_field_valid (char const * p_text, size_t max_length)
{
    size_t length;

    if (NULL == p_text)
    {
        return false;
    }

    length = strlen(p_text);
    if ((0U == length) || (length > max_length))
    {
        return false;
    }

    return ((NULL == strchr(p_text, '\r')) &&
            (NULL == strchr(p_text, '\n')) &&
            (NULL == strchr(p_text, ',')) &&
            (NULL == strchr(p_text, '\'')));
}

static bool da16200_stage2_config_valid (void)
{
    return (da16200_stage2_field_valid(DA16200_MQTT_BROKER_HOST, 128U) &&
            da16200_stage2_field_valid(DA16200_MQTT_SUB_TOPIC, 64U) &&
            da16200_stage2_field_valid(DA16200_MQTT_PUB_TOPIC, 64U) &&
            da16200_stage2_field_valid(DA16200_MQTT_CLIENT_ID, 128U) &&
            (0U != DA16200_MQTT_BROKER_PORT));
}

static uint8_t da16200_execute_formatted_command (char const * p_format,
                                                   char const * p_text,
                                                   uint32_t number)
{
    char command[DA16200_MQTT_COMMAND_SIZE];
    int length;

    if (NULL == p_text)
    {
        return 4U;
    }

    length = snprintf(command, sizeof(command), p_format, p_text, (unsigned long) number);
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

static bool da16200_wait_for_event (volatile uint32_t const * p_event_count,
                                    uint32_t event_count_before,
                                    uint32_t timeout_ms)
{
    for (uint32_t elapsed = 0U; elapsed < timeout_ms; elapsed++)
    {
        da16200_process_rx();
        if (*p_event_count != event_count_before)
        {
            return true;
        }
        R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
    }

    return false;
}

static bool da16200_apply_mqtt_config (void)
{
    static char const stop_command[] = "AT+NWMQCL=0\r\n";
    static char const qos_command[] = "AT+NWMQQOS=0\r\n";
    static char const auto_command[] = "AT+NWMQAUTO=1\r\n";

    if (1U != da16200_execute_command(stop_command, NULL, NULL, 0U,
                                      DA16200_RESPONSE_TIMEOUT_MS))
    {
        return false;
    }
    if (1U != da16200_execute_formatted_command("AT+NWMQBR=%s,%lu\r\n",
                                                 DA16200_MQTT_BROKER_HOST,
                                                 DA16200_MQTT_BROKER_PORT))
    {
        return false;
    }
    if (1U != da16200_execute_formatted_command("AT+NWMQTS=1,%s\r\n",
                                                 DA16200_MQTT_SUB_TOPIC,
                                                 0U))
    {
        return false;
    }
    if (1U != da16200_execute_formatted_command("AT+NWMQTP=%s\r\n",
                                                 DA16200_MQTT_PUB_TOPIC,
                                                 0U))
    {
        return false;
    }
    if (1U != da16200_execute_formatted_command("AT+NWMQCID=%s\r\n",
                                                 DA16200_MQTT_CLIENT_ID,
                                                 0U))
    {
        return false;
    }
    if (1U != da16200_execute_command(qos_command, NULL, NULL, 0U,
                                      DA16200_RESPONSE_TIMEOUT_MS))
    {
        return false;
    }

    return (1U == da16200_execute_command(auto_command, NULL, NULL, 0U,
                                          DA16200_RESPONSE_TIMEOUT_MS));
}

static bool da16200_ensure_mqtt_connected (void)
{
    static char const query_command[] = "AT+NWMQCL\r\n";
    static char const start_command[] = "AT+NWMQCL=1\r\n";
    char response[DA16200_LINE_SIZE];
    uint32_t event_count_before;

    if (1U != da16200_execute_command(query_command,
                                      "+NWMQCL:",
                                      response,
                                      sizeof(response),
                                      DA16200_RESPONSE_TIMEOUT_MS))
    {
        return false;
    }
    if (1U == g_mqtt_client_state)
    {
        return true;
    }

    event_count_before = g_mqtt_client_event_count;
    if (1U != da16200_execute_command(start_command, NULL, NULL, 0U,
                                      DA16200_RESPONSE_TIMEOUT_MS))
    {
        return false;
    }

    if ((g_mqtt_client_event_count == event_count_before) &&
        !da16200_wait_for_event(&g_mqtt_client_event_count,
                                event_count_before,
                                DA16200_MQTT_CONNECT_TIMEOUT_MS))
    {
        return false;
    }

    return (1U == g_mqtt_client_state);
}

static bool da16200_publish_replay_sample (replay_sample_t const * p_sample)
{
    char command[DA16200_MQTT_COMMAND_SIZE];
    uint32_t event_count_before;
    int length;

    if ((NULL == p_sample) || (NULL == p_sample->p_payload) ||
        (NULL != strchr(p_sample->p_payload, '\'')) ||
        (strlen(p_sample->p_payload) > 100U))
    {
        return false;
    }

    length = snprintf(command,
                      sizeof(command),
                      "AT+NWMQMSG='%s',%s,0\r\n",
                      p_sample->p_payload,
                      DA16200_MQTT_PUB_TOPIC);
    if ((length <= 0) || ((size_t) length >= sizeof(command)))
    {
        return false;
    }

    g_mqtt_publish_result = 0U;
    g_mqtt_publish_line[0] = '\0';
    event_count_before = g_mqtt_publish_event_count;
    g_replay_publish_attempts++;

    if (1U != da16200_execute_command(command, NULL, NULL, 0U,
                                      DA16200_RESPONSE_TIMEOUT_MS))
    {
        g_replay_publish_failures++;
        return false;
    }

    if ((g_mqtt_publish_event_count == event_count_before) &&
        !da16200_wait_for_event(&g_mqtt_publish_event_count,
                                event_count_before,
                                DA16200_MQTT_PUBLISH_TIMEOUT_MS))
    {
        g_replay_publish_failures++;
        return false;
    }

    if (1U != g_mqtt_publish_result)
    {
        g_replay_publish_failures++;
        return false;
    }

    g_replay_publish_successes++;
    return true;
}

static uint8_t da16200_run_stage2_replay (void)
{
    if (!da16200_stage2_config_valid())
    {
        return 2U;
    }

    if ((0U != DA16200_STAGE2_APPLY_MQTT_CONFIG) &&
        !da16200_apply_mqtt_config())
    {
        return 3U;
    }

    if (!da16200_ensure_mqtt_connected())
    {
        return 4U;
    }

    for (size_t index = 0U; index < ReplayDataset_Count(); index++)
    {
        replay_sample_t const * p_sample = ReplayDataset_Get(index);
        g_stage2_sample_index = p_sample->sequence;
        if (!da16200_publish_replay_sample(p_sample))
        {
            return 5U;
        }
    }

    g_stage2_sample_index = (uint32_t) ReplayDataset_Count();
    return 1U;
}

#if DA16200_ENABLE_READ_ONLY_DETAILS
static uint8_t da16200_query_wifi_mode (void)
{
    static char const command[] = "AT+WFMODE\r\n";

    return da16200_execute_command(command,
                                   "+WFMODE:",
                                   g_wifi_mode_line,
                                   sizeof(g_wifi_mode_line),
                                   DA16200_RESPONSE_TIMEOUT_MS);
}

static uint8_t da16200_query_country_code (void)
{
    static char const command[] = "AT+WFCC\r\n";

    return da16200_execute_command(command,
                                   "+WFCC:",
                                   g_country_line,
                                   sizeof(g_country_line),
                                   DA16200_RESPONSE_TIMEOUT_MS);
}
#endif

#if DA16200_ENABLE_ACTIVE_CONTROL
static uint8_t da16200_set_country_code_cn (void)
{
    static char const command[] = "AT+WFCC=CN\r\n";

    return da16200_execute_command(command,
                                   NULL,
                                   NULL,
                                   0U,
                                   DA16200_RESPONSE_TIMEOUT_MS);
}
#endif

#if DA16200_ENABLE_READ_ONLY_DETAILS
static uint8_t da16200_query_dpm (void)
{
    static char const command[] = "AT+DPM=?\r\n";

    return da16200_execute_command(command,
                                   "+DPM:",
                                   g_dpm_line,
                                   sizeof(g_dpm_line),
                                   DA16200_RESPONSE_TIMEOUT_MS);
}
#endif

#if DA16200_ENABLE_ACTIVE_CONTROL
static uint8_t da16200_disable_dpm_nvm_only (void)
{
    static char const command[] = "AT+DPM=0,1\r\n";

    return da16200_execute_command(command,
                                   NULL,
                                   NULL,
                                   0U,
                                   DA16200_RESPONSE_TIMEOUT_MS);
}

static uint8_t da16200_cancel_ap_connection (void)
{
    static char const command[] = "AT+WFQAP\r\n";

    return da16200_execute_command(command,
                                   NULL,
                                   NULL,
                                   0U,
                                   DA16200_RESPONSE_TIMEOUT_MS);
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
#endif
/*******************************************************************************************************************//**
 * main() is generated by the RA Configuration editor and is used to generate threads if an RTOS is used.  This function
 * is called by main() when no RTOS is used.
 **********************************************************************************************************************/
void hal_entry(void)
{
    fsp_err_t err;
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
    g_wakeup_wait_ms = 0U;
    g_wakeup_uart_event = 0U;
    g_wakeup_rx_seen = 0U;
    g_wakeup_stage = 0U;
    g_wakeup_gpio_result = (uint32_t) FSP_SUCCESS;
    g_rx_line_length = 0U;
    g_rx_line_discarding = false;
    g_tx_complete = 0U;
    g_uart_error = 0U;
    g_at_attempt = 0U;
    g_at_pending = 0U;
    g_at_expected_seen = 0U;
    g_at_sync_result = 0U;
    g_dpm_handshake_stage = 0U;
    g_mcuwudone_result = 0U;
    g_clear_dpm_sleep_result = 0U;
    g_set_dpm_sleep_result = 0U;
    g_version_result = 0U;
    g_wifi_mode_result = 0U;
    g_country_result = 0U;
    g_country_set_result = 0U;
    g_dpm_result = 0U;
    g_dpm_set_result = 0U;
    g_cancel_ap_result = 0U;
    g_startup_cancel_attempts = 0U;
    g_scan_result = 0U;
    g_scan_active = 0U;
    g_join_command_result = 0U;
    g_join_result = 0U;
    g_join_active = 0U;
    g_target_ap_found = 0U;
    g_scan_ap_count = 0U;
    g_stage2_result = 0U;
    g_stage2_sample_index = 0U;
    g_replay_publish_attempts = 0U;
    g_replay_publish_successes = 0U;
    g_replay_publish_failures = 0U;
    g_mqtt_client_state = 0U;
    g_mqtt_publish_result = 0U;
    g_mqtt_client_event_count = 0U;
    g_mqtt_publish_event_count = 0U;
    g_version_line[0] = '\0';
    g_wifi_mode_line[0] = '\0';
    g_country_line[0] = '\0';
    g_dpm_line[0] = '\0';
    g_scan_first_line[0] = '\0';
    g_target_ap_line[0] = '\0';
    g_join_line[0] = '\0';
    g_scan_error_line[0] = '\0';
    g_mqtt_client_line[0] = '\0';
    g_mqtt_publish_line[0] = '\0';
    g_last_rx_line[0] = '\0';
    gp_at_expected_prefix = NULL;
    gp_at_response_line = NULL;
    g_at_response_line_size = 0U;

    err = g_uart0.p_api->open(g_uart0.p_ctrl, g_uart0.p_cfg);

    if (FSP_SUCCESS == err)
    {
        /* Stage 1: validate the manually confirmed RTC_WAKE_UP polarity and
         * pulse width. Do not change DPM, Wi-Fi, country code, or NVRAM. */
        do
        {
            uint32_t wakeup_indication_before = g_wakeup_indication_count;

            g_dpm_handshake_stage = 0U;
            g_mcuwudone_result = 0U;
            g_clear_dpm_sleep_result = 0U;
            g_at_sync_result = 0U;
            g_set_dpm_sleep_result = 0U;

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

                        if ((1U == g_at_sync_result) &&
                            (0U != DA16200_STAGE2_ENABLE))
                        {
                            g_stage2_result = da16200_run_stage2_replay();
                        }

                        /* Restore normal DPM entry even when the basic AT
                         * validation or optional Stage 2 replay fails after
                         * sleep was cleared. */
                        g_dpm_handshake_stage = 4U;
                        g_set_dpm_sleep_result = da16200_set_dpm_sleep_ext();
                    }

                    if ((1U == g_mcuwudone_result) &&
                        (1U == g_clear_dpm_sleep_result) &&
                        (1U == g_at_sync_result) &&
                        (1U == g_set_dpm_sleep_result))
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
                    g_at_sync_result = 3U;
                    g_dpm_handshake_stage = 6U;
                    g_wakeup_stage = 7U;
                }
            }
            else
            {
                g_at_sync_result = 4U;
                g_dpm_handshake_stage = 6U;
            }

            /* No detailed query or persistent Wi-Fi/DPM configuration command
             * is sent in this hardware wake-up validation stage. */
            g_version_result = 0U;
            g_wifi_mode_result = 0U;
            g_country_result = 0U;
            g_dpm_result = 0U;
            g_country_set_result = 0U;
            g_dpm_set_result = 0U;
            g_cancel_ap_result = 0U;
            g_startup_cancel_attempts = 0U;
            g_scan_result = 0U;
            g_join_command_result = 0U;
            g_join_result = 0U;

            g_uart_error = (6U == g_wakeup_stage) ? 0U : 1U;

            if (0U != g_uart_error)
            {
                /* One short LED pulse per failed attempt, then retry. */
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_03, BSP_IO_LEVEL_HIGH);
                da16200_service_delay(100U);
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_03, BSP_IO_LEVEL_LOW);
                da16200_service_delay(DA16200_RETRY_INTERVAL_MS);
            }
        } while (0U != g_uart_error);
    }
    else
    {
        g_at_result = 4U;
    }

    g_uart_error = (6U == g_wakeup_stage) ? 0U : 1U;

    while (1)
    {
        if ((0U == g_uart_error) &&
            ((0U == DA16200_STAGE2_ENABLE) || (1U == g_stage2_result)))
        {
            /* 通信成功：LED 500 ms 周期闪烁 */
            R_IOPORT_PinWrite(&g_ioport_ctrl,
                              BSP_IO_PORT_01_PIN_03,
                              BSP_IO_LEVEL_HIGH);
            da16200_service_delay(500U);

            R_IOPORT_PinWrite(&g_ioport_ctrl,
                              BSP_IO_PORT_01_PIN_03,
                              BSP_IO_LEVEL_LOW);
            da16200_service_delay(500U);
        }
        else
        {
            /* 通信失败：LED 快速闪烁 */
            R_IOPORT_PinWrite(&g_ioport_ctrl,
                              BSP_IO_PORT_01_PIN_03,
                              BSP_IO_LEVEL_HIGH);
            da16200_service_delay(100U);

            R_IOPORT_PinWrite(&g_ioport_ctrl,
                              BSP_IO_PORT_01_PIN_03,
                              BSP_IO_LEVEL_LOW);
            da16200_service_delay(100U);
        }
    }

    /* Wake up 2nd core if this is first core and we are inside a multicore project. */
#if (0 == _RA_CORE) && (1 == BSP_MULTICORE_PROJECT) && !BSP_TZ_NONSECURE_BUILD

#if BSP_TZ_SECURE_BUILD
    /* Take semaphore so 2nd core can clear it */
    R_BSP_IpcSemaphoreTake(&g_core_start_semaphore);
#endif

    R_BSP_SecondaryCoreStart();

#if BSP_TZ_SECURE_BUILD
    /* Wait for 2nd core to start and clear semaphore */
    while(FSP_ERR_IN_USE == R_BSP_IpcSemaphoreTake(&g_core_start_semaphore))
    {
        ;
    }
#endif
#endif

#if (1 == _RA_CORE) && (1 == BSP_MULTICORE_PROJECT) && BSP_TZ_SECURE_BUILD
    /* Signal to 1st core that 2nd core has started */
    R_BSP_IpcSemaphoreGive(&g_core_start_semaphore);
#endif

#if BSP_TZ_SECURE_BUILD
    /* Enter non-secure code */
    R_BSP_NonSecureEnter();
#endif
}

#if BSP_TZ_SECURE_BUILD

FSP_CPP_HEADER
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ();

/* Trustzone Secure Projects require at least one nonsecure callable function in order to build (Remove this if it is not required to build). */
BSP_CMSE_NONSECURE_ENTRY void template_nonsecure_callable ()
{

}
FSP_CPP_FOOTER

#endif
