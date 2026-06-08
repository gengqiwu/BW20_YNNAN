#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct
{
	uint8_t	*buffer;		
	int32_t	read_offset;	
	int32_t	write_offset;	
	int32_t	valid_size;	
	int32_t	total_size;		
}ring_buffer_t;

void ring_buffer_init(ring_buffer_t *ring_buffer, int32_t buff_size);
void ring_buffer_deinit(ring_buffer_t *ring_buffer);
int32_t ring_buffer_total_size(ring_buffer_t *ring_buffer);
int32_t ring_buffer_valid_size(ring_buffer_t *ring_buffer);
uint8_t ring_buffer_is_empty(ring_buffer_t *ring_buffer);
uint8_t ring_buffer_is_full(ring_buffer_t *ring_buffer);
void ring_buffer_read(ring_buffer_t *ring_buffer, void *buff, int32_t size);
void ring_buffer_write(ring_buffer_t *ring_buffer, void *buffer_to_write, int32_t size);
uint32_t ring_buffer_real_read(ring_buffer_t *ring_buffer, void *buffer, int32_t size);
uint32_t ring_buffer_real_write(ring_buffer_t *ring_buffer, void *buffer, int32_t size);

#ifdef __cplusplus
}
#endif
	
#endif
