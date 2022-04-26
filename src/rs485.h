#include <Arduino.h>

class Rs485 : public HardwareSerial {
public:
    using HardwareSerial::HardwareSerial;

    static int _tx_complete_irq_RS(serial_t *obj);
    size_t write(const uint8_t *buffer, size_t size) override; 
};

int Rs485::_tx_complete_irq_RS(serial_t *obj)
{
  size_t remaining_data;
  digitalWrite(PE10, HIGH);
  // previous HAL transfer is finished, move tail pointer accordingly
  obj->tx_tail = (obj->tx_tail + obj->tx_size) % SERIAL_TX_BUFFER_SIZE;

  // If buffer is not empty (head != tail), send remaining data
  if (obj->tx_head != obj->tx_tail) {
    remaining_data = (SERIAL_TX_BUFFER_SIZE + obj->tx_head - obj->tx_tail)
                     % SERIAL_TX_BUFFER_SIZE;
    // Limit the next transmission to the buffer end
    // because HAL is not able to manage rollover
    obj->tx_size = min(remaining_data,
                       (size_t)(SERIAL_TX_BUFFER_SIZE - obj->tx_tail));
    uart_attach_tx_callback(obj, _tx_complete_irq_RS, obj->tx_size);
    return -1;
  }
  return 0;
}

size_t Rs485::write(const uint8_t *buffer, size_t size)
{
  size_t size_intermediate;
  size_t ret = size;
  size_t available = availableForWrite();
  size_t available_till_buffer_end = SERIAL_TX_BUFFER_SIZE - _serial.tx_head;

  _written = true;

  // If the output buffer is full, there's nothing for it other than to
  // wait for the interrupt handler to free space
  while (!availableForWrite()) {
    // nop, the interrupt handler will free up space for us
  }

  // HAL doesn't manage rollover, so split transfer till end of TX buffer
  // Also, split transfer according to available space in buffer
  while ((size > available_till_buffer_end) || (size > available)) {
    size_intermediate = min(available, available_till_buffer_end);
    write(buffer, size_intermediate);
    size -= size_intermediate;
    buffer += size_intermediate;
    available = availableForWrite();
    available_till_buffer_end = SERIAL_TX_BUFFER_SIZE - _serial.tx_head;
  }

  // Copy data to buffer. Take into account rollover if necessary.
  if (_serial.tx_head + size <= SERIAL_TX_BUFFER_SIZE) {
    memcpy(&_serial.tx_buff[_serial.tx_head], buffer, size);
    size_intermediate = size;
  } else {
    // memcpy till end of buffer then continue memcpy from beginning of buffer
    size_intermediate = SERIAL_TX_BUFFER_SIZE - _serial.tx_head;
    memcpy(&_serial.tx_buff[_serial.tx_head], buffer, size_intermediate);
    memcpy(&_serial.tx_buff[0], buffer + size_intermediate,
           size - size_intermediate);
  }

  // Data are copied to buffer, move head pointer accordingly
  _serial.tx_head = (_serial.tx_head + size) % SERIAL_TX_BUFFER_SIZE;

  // Transfer data with HAL only is there is no TX data transfer ongoing
  // otherwise, data transfer will be done asynchronously from callback
  if (!serial_tx_active(&_serial)) {
    // note: tx_size correspond to size of HAL data transfer,
    // not the total amount of data in the buffer.
    // To compute size of data in buffer compare head and tail
    _serial.tx_size = size_intermediate;
    uart_attach_tx_callback(&_serial, _tx_complete_irq_RS, size_intermediate);
  }

  /* There is no real error management so just return transfer size requested*/
  return ret;
}