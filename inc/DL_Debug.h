#ifndef DL_DEBUG_H
#define DL_DEBUG_H

#pragma GCC diagnostic ignored "-Wunused-function"

#ifndef DEBUG_D
#define DBG(x)
#define DBGF(x, ...)
#define DBGH(x, len)
#define DBGC(x, len)
#endif



#ifdef DEBUG_D
    #include "main.h"
    #include "string.h"
    #include "stdio.h"
    #include "stdarg.h"
    #include "stm32f411xe.h"
    #ifndef DEBUG_UART 
        #warning You must define DEBUG_UART
        #define DEBUG_UART (uint32_t*)0x40011000
    #endif

    #define DBG(x) debug_send(x)                        // Debug print string
    #define DBGF(x, ...) debug_send(x, __VA_ARGS__)     // Debug print with format
    #define DBGH(x, len) debug_sendBytesAsHex(x, len)   // Debug print formatted as HEX
    #define DBGC(x, len) debug_sendBytesAsChar(x, len)

    static char d_buffer[512];

    static void debug_send(char *s, ...){
        va_list args;
        va_start(args, s);
        vsprintf(d_buffer, s, args);
        va_end(args);
        //sprintf(d_buffer, s);
        unsigned int len = strlen(d_buffer);
        for(int i = 0; i < len; i++){
            ((USART_TypeDef*) DEBUG_UART)->DR = d_buffer[i];
            while(!(((USART_TypeDef*) DEBUG_UART)->SR & USART_SR_TXE));
        }
        while(!(((USART_TypeDef*) DEBUG_UART)->SR & USART_SR_TXE));
    }

    static void debug_sendBytesAsHex(char *s, int len){
        for (unsigned int i = 0; i < len; i++){
            char buff[3] = {0};
            sprintf(buff, "%x", s[i]);
            for(int j = 0; j < 2; j++){
                ((USART_TypeDef*) DEBUG_UART)->DR = buff[j];
                while(!(((USART_TypeDef*) DEBUG_UART)->SR & USART_SR_TXE));
            }
            ((USART_TypeDef*) DEBUG_UART)->DR = ' ';
            while(!(((USART_TypeDef*) DEBUG_UART)->SR & USART_SR_TXE));
            if ((i + 1) % 32 == 0) {
                ((USART_TypeDef*) DEBUG_UART)->DR = '\n';
                while(!(((USART_TypeDef*) DEBUG_UART)->SR & USART_SR_TXE));
            }
        }
        ((USART_TypeDef*) DEBUG_UART)->DR = '\n';
        while(!(((USART_TypeDef*) DEBUG_UART)->SR & USART_SR_TXE));
    }

    static void debug_sendBytesAsChar(char *s, int len){
        for (int i = 0; i < len; i++){
            char buff[3] = {0};
            sprintf(buff, "%c", s[i]);
            for(int j = 0; j < 2; j++){
                ((USART_TypeDef*) DEBUG_UART)->DR = buff[j];
                while(!(((USART_TypeDef*) DEBUG_UART)->SR & USART_SR_TXE));
            }
            ((USART_TypeDef*) DEBUG_UART)->DR = ' ';
            while(!(((USART_TypeDef*) DEBUG_UART)->SR & USART_SR_TXE));
            if ((i + 1) % 32 == 0) {
                ((USART_TypeDef*) DEBUG_UART)->DR = '\n';
                while(!(((USART_TypeDef*) DEBUG_UART)->SR & USART_SR_TXE));
            }
        }
        ((USART_TypeDef*) DEBUG_UART)->DR = '\n';
        while(!(((USART_TypeDef*) DEBUG_UART)->SR & USART_SR_TXE));
    }

    


#endif



#endif