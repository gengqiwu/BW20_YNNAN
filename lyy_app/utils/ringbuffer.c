#include "ringBuffer.h"

#include "HAL_Log.h"
#include "HAL_System.h"


/*********************************************************************************************
*@description:初始化环形缓存区
*@Author: chenyj
*@param[in]: ring_buffer：环形缓存区指针，buff_size：缓存区大小
*@return: 无
*@others 
*********************************************************************************************/
void ring_buffer_init(ring_buffer_t *ring_buffer, int32_t buff_size)
{
	ring_buffer->buffer = pvPortMalloc(buff_size);
	if(ring_buffer->buffer == NULL){
		LYY_LOGE("malloc ringbuf fail!\t%d",buff_size);
		ring_buffer = NULL;
		return;
	}
	
	memset(ring_buffer->buffer, 0, buff_size);
	ring_buffer->read_offset = 0;
	ring_buffer->write_offset = 0;
	ring_buffer->valid_size = 0;
	ring_buffer->total_size = buff_size;
	LYY_LOGD("ring_buffer_init [%d]....ok!", buff_size);
}

/*********************************************************************************************
*@description:释放环形缓存区
*@Author: chenyj
*@param[in]: ring_buffer：环形缓存区指针
*@return: 无
*@others 
*********************************************************************************************/
void ring_buffer_deinit(ring_buffer_t *ring_buffer)
{
	if(ring_buffer == NULL){
		return;
	}
	
	if(ring_buffer->buffer != NULL){
		vPortFree(ring_buffer->buffer);
	}
	
	memset(ring_buffer, 0, sizeof(ring_buffer_t));
}

/*********************************************************************************************
*@description:获取环形缓存区大小
*@Author: chenyj
*@param[in]: ring_buffer
*@return: ring_buffer->total_size:总大小
*@others 
*********************************************************************************************/
int32_t ring_buffer_total_size(ring_buffer_t *ring_buffer)
{
	if(ring_buffer == NULL){
		return 0;
	}
	
	return ring_buffer->total_size;
}

/*********************************************************************************************
*@description:获取缓存区有效数据长度
*@Author: chenyj
*@param[in]: ring_buffer
*@return: 1:已满，FALSE：不满
*@others 
*********************************************************************************************/
int32_t ring_buffer_valid_size(ring_buffer_t *ring_buffer)
{
	if(ring_buffer == NULL){
		return 0;
	}
	
	return ring_buffer->valid_size;
}

/*********************************************************************************************
*@description:判断环形缓存区是否为空
*@Author: chenyj
*@param[in]: ring_buffer
*@return: 1:为空，FALSE：非空
*@others 
*********************************************************************************************/
uint8_t ring_buffer_is_empty(ring_buffer_t *ring_buffer)
{
	if(ring_buffer == NULL){
		return 0;
	}
	
	if(ring_buffer->valid_size == 0){
		return 1;
	}
	
	return 0;
}

/*********************************************************************************************
*@description:判断环形缓存区是否已满
*@Author: chenyj
*@param[in]: ring_buffer
*@return: 1:已满，FALSE：不满
*@others 
*********************************************************************************************/
uint8_t ring_buffer_is_full(ring_buffer_t *ring_buffer)
{
	if(ring_buffer == NULL){
		return 0;
	}
	
	if(ring_buffer->valid_size == ring_buffer->total_size){
		return 1;
	}
	
	return 0;
}

/*********************************************************************************************
*@description:写入环形缓存区
*@Author: chenyj
*@param[in]: ring_buffer：环形缓存区指针,buffer_to_write:需要写入的缓存区指针，size：写入大小
*@return: 无
*@others 
*********************************************************************************************/
void ring_buffer_write(ring_buffer_t *ring_buffer, void *buffer_to_write, int32_t size)
{
	int32_t write_offset = ring_buffer->write_offset;
	int32_t total_size = ring_buffer->total_size;
	int32_t first_write_size = 0;
	
	//ring_buffer->buffer未使用的总大小比需要写入的size小
	if(ring_buffer->valid_size + size > total_size){
		LYY_LOGE("ring buffer is to short to save buff");
		LYY_LOGE("total_size:%d valid_size:%d size:%d", ring_buffer->total_size, ring_buffer->valid_size, size);
		return;
	}

	//ring_buffer->buffer的后段未写入的空间不小于size
	if(size + write_offset <= total_size){
		memcpy(ring_buffer->buffer + write_offset, buffer_to_write, size);
	}else{//ring_buffer->buffer的后段未写入的空间小于size,这时候需要先在后面写入一部分，然后返回头部，从前面接着写入
		first_write_size = total_size - write_offset;
		memcpy(ring_buffer->buffer + write_offset, buffer_to_write, first_write_size);
		memcpy(ring_buffer->buffer, ((uint8_t *)buffer_to_write + first_write_size), size - first_write_size);
	}
	ring_buffer->write_offset += size;
	ring_buffer->write_offset %= total_size;
	ring_buffer->valid_size	+= size;
}

/*********************************************************************************************
*@description:读取环形缓存区
*@Author: chenyj
*@param[in]: ring_buffer：环形缓存区指针,buff:读取到的缓存区指针，size：读取大小
*@return: 无
*@others 
*********************************************************************************************/
void ring_buffer_read(ring_buffer_t *ring_buffer, void *buff, int32_t size)
{
	int32_t read_offset = ring_buffer->read_offset;
	int32_t total_size = ring_buffer->total_size;
	int32_t first_read_size = 0;

	if(size > ring_buffer->valid_size){
		LYY_LOGE("valid size[%d] < read size[%d]", ring_buffer->valid_size, size);
		return;
	}

	if(total_size - read_offset >= size){
		memcpy(buff, ring_buffer->buffer + read_offset, size);
	}else{
		first_read_size = total_size - read_offset;
		memcpy(buff, ring_buffer->buffer + read_offset, first_read_size);
		memcpy(((uint8_t *)buff + first_read_size), ring_buffer->buffer, size - first_read_size);
	}
	
	ring_buffer->read_offset += size;
	ring_buffer->read_offset %= total_size;
	ring_buffer->valid_size -= size;
}

/*********************************************************************************************
*@description:根据实际可用大小写入环形缓存区
*@Author: chenyj
*@param[in]: ring_buffer：环形缓存区指针,buffer_to_write:需要写入的缓存区指针，size：写入大小
*@return: 返回真实写入长度
*@others 根据缓存区可用大小来操作，不可覆盖写。
*********************************************************************************************/
uint32_t ring_buffer_real_write(ring_buffer_t *ring_buffer, void *buffer, int32_t size)
{
	int32_t real_size = 0;
	
	if( ring_buffer_is_full(ring_buffer) ){
		return 0;
	}
	
	if(size > (ring_buffer->total_size - ring_buffer->valid_size)){
		real_size = ring_buffer->total_size - ring_buffer->valid_size;
		LYY_LOGW("ring buffer write size[%d] > used size[%d]",size,ring_buffer->total_size - ring_buffer->valid_size);
	}else{
		real_size = size;
	}
	
	ring_buffer_write(ring_buffer, buffer, real_size);
	
	return real_size;
}

/*********************************************************************************************
*@description:根据实际有效大小读取环形缓存区
*@Author: chenyj
*@param[in]: ring_buffer：环形缓存区指针,buff:读取到的缓存区指针，size：读取大小
*@return: 返回真实读取长度
*@others 
*********************************************************************************************/
uint32_t ring_buffer_real_read(ring_buffer_t *ring_buffer, void *buffer, int32_t size)
{
	int32_t real_size = 0;
	
	if( ring_buffer_is_empty(ring_buffer) ){
		return 0;
	}
	
	if(size > ring_buffer->valid_size){
		real_size = ring_buffer->valid_size;
		//LYY_LOGW("ring buffer read size[%d] > valid size[%d]",size,ring_buffer->valid_size);
	}else{
		real_size = size;
	}
	
	ring_buffer_read(ring_buffer, buffer, real_size);
	
	return real_size;
}

