import codecs
import base64
import struct

# Author: Richard Wang

def read_int(buf, bit, length):
    total_bits_remaining = length
    val = 0
    bit_start = bit

    while total_bits_remaining > 0:
        byte = bit_start // 8
        control_bits = buf[byte] 
        local_bit_start = bit_start % 8
        bit_rem_in_byte = 8 - local_bit_start
        n_bits_filling = 0

        if local_bit_start != 0:
            n_bits_filling = min(bit_rem_in_byte, total_bits_remaining)
            keep_bits = 0
            for i in range(local_bit_start, min(local_bit_start + n_bits_filling + 1, 8)):
                keep_bits |= (0x1 << (7-i))
            control_bits &= keep_bits
            if local_bit_start > total_bits_remaining:
                control_bits >>= bit_rem_in_byte - total_bits_remaining
            else:
                shift = total_bits_remaining - n_bits_filling
                control_bits <<= shift

        else: # bit_start % 8 == 0
            n_bits_filling = min(8, total_bits_remaining)
            if total_bits_remaining > n_bits_filling:
                control_bits <<= total_bits_remaining - n_bits_filling
            else:
                control_bits >>= bit_rem_in_byte - n_bits_filling

        val |= control_bits
        total_bits_remaining -= n_bits_filling
        bit_start += n_bits_filling
        

    return val

def getValue(timestamps, d):
    return timestamps[-1] + timestamps[-1] - timestamps[-2] + d

# ------ function --------
def decode(payload, values):
    buf = bytearray(payload)
    timestamps = []
    read_bit = 0
    if values <= 0:
        return timestamps
    empty_bits = read_int(buf, 0, 3)
    read_bit += 3
    first = read_int(buf, read_bit, 32)
    read_bit += 32
    timestamps.append(first)
    if values == 1:
        return timestamps
    first_delta = read_int(buf, read_bit, 14)
    read_bit += 14
    timestamps.append(first + first_delta)
    for i in range(2, values):
        found = 0
        for j in range(4):
            b = read_int(buf, read_bit, 1)
            read_bit += 1
            if b == 0:
                if j == 0:
                    timestamps.append(getValue(timestamps, 0))
                elif j == 1:
                    v = read_int(buf, read_bit, 7)
                    read_bit += 7
                    if v > 64:
                        v -= 128
                    timestamps.append(getValue(timestamps, v))
                elif j == 2:
                    v = read_int(buf, read_bit, 9)
                    read_bit += 9
                    if v > 256:
                        v -= 256
                    timestamps.append(getValue(timestamps, v))
                elif j == 3:
                    v = read_int(buf, read_bit, 12)
                    read_bit += 12
                    if v > 2048:
                        v -= 4096
                    timestamps.append(getValue(timestamps, v))
                found = 1
                break
        if found == 0:
            v = read_int(buf, read_bit, 32)
            read_bit += 32
            if v >= 2147483648:
                v -= 4294967296
            timestamps.append(getValue(timestamps, v))
    return timestamps
    
        
# Encode binary data to a base 64 string
binary_data = b'\x60\x00\x00\x0C\x80\x00\xC0\x50\x1B\xF0'
# Use the codecs module to encode
base64_data = codecs.encode(binary_data, 'base64')
payload = base64.b64decode(base64_data)

print(decode(payload, 6))