#include "da16200.h"
#include "app_config.h"
#include "wifi_credentials.h"
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
static bool g_uart_opened;
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

static bool da16200_is_sensitive_command_echo (char const * p_line)
{
    return (0 == strncmp(p_line, "AT+WFJAPA=", 10U));
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

void DA16200_ServiceDelay (uint32_t delay_ms)
{
    da16200_service_delay(delay_ms);
}

bool DA16200_IsReady (void)
{
    return ((0U == g_uart_error) && (1U == g_wifi_ready));
}
