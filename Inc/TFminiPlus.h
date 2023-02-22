/**
*	\file         tamplate.h
*	\brief        tamplate.
*	
*	\author       Roman Garanin
*/

#ifndef TAMPLATE_H
#define TAMPLATE_H
#define _CRT_SECURE_NO_WARNINGS

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdlib.h"
#include "stdbool.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"

extern uint16_t d;
extern uint16_t s;
extern uint16_t t;
/**
 * \Brief Seaches a header (0x59, 0x59, ...) in sequense of bytes.
 * \param seq Pointer to sequense of bytes.
 * \param header Pointer to pointer of header.
 * \param len Lenght of sequents of bytes.
 * \return Lenght of data after header (header included).
 */

void input_data(uint8_t * data, uint8_t len);
uint8_t find_header(uint8_t* seq, uint8_t** header, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif // !TAMPLATE_H
