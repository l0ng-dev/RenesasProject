#include "hal_data.h"
#include <string.h>
#if (1 == BSP_MULTICORE_PROJECT) && BSP_TZ_SECURE_BUILD
bsp_ipc_semaphore_handle_t g_core_start_semaphore =
{
    .semaphore_num = 0
};
#endif
#define DA16200_RX_BUFFER_SIZE       (256U)
#define DA16200_LINE_SIZE            (96U)
#define DA16200_MAX_ATTEMPTS         (1U)
#define DA16200_RESPONSE_TIMEOUT_MS  (1500U)
#define DA16200_RETRY_INTERVAL_MS    (2000U)

/* RX callback writes g_rx_head; the foreground parser writes g_rx_tail. */
static uint8_t g_rx_buffer[DA16200_RX_BUFFER_SIZE];
static volatile uint32_t g_rx_head;
static volatile uint32_t g_rx_tail;
static volatile uint32_t g_rx_length;
static volatile uint32_t g_rx_dropped;
static volatile uint32_t g_last_uart_event;
static volatile uint32_t g_at_attempt;
/* 0: pending, 1: +VER and OK, 2: ERROR, 3: timeout,
 * 4: TX failure, 5: RX buffer full, 6: UART RX error. */
static volatile uint8_t g_at_result;
static volatile uint8_t g_tx_complete;
static volatile uint8_t g_uart_error;
static char g_version_line[DA16200_LINE_SIZE];

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
    }

    switch (p_args->event)
    {
        case UART_EVENT_RX_CHAR:
        {
            uint32_t next = (g_rx_head + 1U) % DA16200_RX_BUFFER_SIZE;

            if (next != g_rx_tail)
            {
                g_rx_buffer[g_rx_head] = (uint8_t) p_args->data;
                g_rx_head = next;
                g_rx_length++;
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

static uint8_t da16200_check_line (char const * p_line)
{
    if (0 == strncmp(p_line, "+VER:", 5U))
    {
        size_t length = strlen(p_line);
        memcpy(g_version_line, p_line, length + 1U);
        return 0U;
    }

    if (0 == strcmp(p_line, "OK"))
    {
        return ('\0' != g_version_line[0]) ? 1U : 0U;
    }

    if (0 == strncmp(p_line, "ERROR", 5U))
    {
        return 2U;
    }

    return 0U;
}

static uint8_t da16200_query_version (void)
{
    static uint8_t const command[] = "AT+VER\r\n";
    char line[DA16200_LINE_SIZE];

    for (uint32_t attempt = 0U; attempt < DA16200_MAX_ATTEMPTS; attempt++)
    {
        uint32_t line_length = 0U;
        uint32_t dropped_before;
        uint8_t result = 3U;

        g_at_attempt++;
        g_rx_tail = g_rx_head; /* Discard start-up and previous-attempt messages. */
        g_version_line[0] = '\0';
        g_last_uart_event = 0U;
        dropped_before = g_rx_dropped;
        g_tx_complete = 0U;

        /* Keep the source buffer valid until UART_EVENT_TX_COMPLETE. */
        if (FSP_SUCCESS != g_uart0.p_api->write(g_uart0.p_ctrl, command, sizeof(command) - 1U))
        {
            return 4U;
        }

        uint32_t tx_timeout = 100U;
        while ((0U == g_tx_complete) && tx_timeout--)
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        }

        if (0U == g_tx_complete)
        {
            return 4U;
        }

        for (uint32_t elapsed = 0U; elapsed < DA16200_RESPONSE_TIMEOUT_MS; elapsed++)
        {
            uint8_t byte;

            while (da16200_read_byte(&byte))
            {
                if (('\r' == byte) || ('\n' == byte))
                {
                    if (line_length > 0U)
                    {
                        uint8_t line_result;

                        line[line_length] = '\0';
                        line_result = da16200_check_line(line);
                        line_length = 0U;
                        if (0U != line_result)
                        {
                            result = line_result;
                            break;
                        }
                    }
                }
                else if ((byte >= 0x20U) && (byte <= 0x7EU))
                {
                    if (line_length < (sizeof(line) - 1U))
                    {
                        line[line_length++] = (char) byte;
                    }
                    else
                    {
                        line_length = 0U;
                    }
                }
                else
                {
                    line_length = 0U; /* Resynchronize after a non-ASCII byte. */
                }
            }

            if (1U == result)
            {
                return result;
            }

            if (2U == result)
            {
                break; /* Retry after an explicit module ERROR response. */
            }

            if (g_rx_dropped != dropped_before)
            {
                result = 5U;
                break;
            }

            if (0U != g_last_uart_event)
            {
                result = 6U;
                break;
            }

            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);
        }

        g_at_result = result;
        if ((attempt + 1U) < DA16200_MAX_ATTEMPTS)
        {
            R_BSP_SoftwareDelay(500U, BSP_DELAY_UNITS_MILLISECONDS);
        }
    }

    return g_at_result;
}
/*******************************************************************************************************************//**
 * main() is generated by the RA Configuration editor and is used to generate threads if an RTOS is used.  This function
 * is called by main() when no RTOS is used.
 **********************************************************************************************************************/
void hal_entry(void)
{
    fsp_err_t err;
    g_rx_length = 0U;
    g_tx_complete = 0U;
    g_uart_error = 0U;
    g_at_attempt = 0U;

    err = g_uart0.p_api->open(g_uart0.p_ctrl, g_uart0.p_cfg);

    if (FSP_SUCCESS == err)
    {
        /* Keep probing until DA16200 is ready; a fixed five-attempt window can
         * expire before the module boots or wakes. Only +VER followed by OK
         * is accepted as success. */
        do
        {
            g_at_result = da16200_query_version();
            g_uart_error = (1U == g_at_result) ? 0U : 1U;

            if (0U != g_uart_error)
            {
                /* One short LED pulse per failed attempt, then retry. */
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_03, BSP_IO_LEVEL_HIGH);
                R_BSP_SoftwareDelay(100U, BSP_DELAY_UNITS_MILLISECONDS);
                R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_03, BSP_IO_LEVEL_LOW);
                R_BSP_SoftwareDelay(DA16200_RETRY_INTERVAL_MS, BSP_DELAY_UNITS_MILLISECONDS);
            }
        } while (0U != g_uart_error);
    }
    else
    {
        g_at_result = 4U;
    }

    g_uart_error = (1U == g_at_result) ? 0U : 1U;

    while (1)
    {
        if (0U == g_uart_error)
        {
            /* 通信成功：LED 500 ms 周期闪烁 */
            R_IOPORT_PinWrite(&g_ioport_ctrl,
                              BSP_IO_PORT_01_PIN_03,
                              BSP_IO_LEVEL_HIGH);
            R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);

            R_IOPORT_PinWrite(&g_ioport_ctrl,
                              BSP_IO_PORT_01_PIN_03,
                              BSP_IO_LEVEL_LOW);
            R_BSP_SoftwareDelay(500, BSP_DELAY_UNITS_MILLISECONDS);
        }
        else
        {
            /* 通信失败：LED 快速闪烁 */
            R_IOPORT_PinWrite(&g_ioport_ctrl,
                              BSP_IO_PORT_01_PIN_03,
                              BSP_IO_LEVEL_HIGH);
            R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);

            R_IOPORT_PinWrite(&g_ioport_ctrl,
                              BSP_IO_PORT_01_PIN_03,
                              BSP_IO_LEVEL_LOW);
            R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MILLISECONDS);
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
