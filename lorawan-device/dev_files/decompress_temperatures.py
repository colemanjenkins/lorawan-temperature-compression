import codecs
import base64
import struct

# Author: Coleman Jenkins

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

# ------ function --------
def decode(payload):
    buf = bytearray(payload)
    float_vals = []
    read_bit = 0

    empty_bits = read_int(buf, 0, 3)
    read_bit += 3

    float_vals.append(read_int(buf, 3, 32))
    read_bit += 32

    state = "store"
    leading_bits = 0
    sig_figs = 0
    while(read_bit < len(buf)*8 - empty_bits):
        if state == "store":
            store_bit = read_int(buf, read_bit, 1)
            read_bit += 1
            if store_bit == 0:
                float_vals.append(float_vals[-1])
                state = "store"
            else: # store bit is '1'
                state = "control"
        if state == "control":
            control_bit = read_int(buf, read_bit, 1)
            read_bit += 1
            if control_bit == 1:
                leading_bits = read_int(buf, read_bit, 5)
                read_bit += 5
                sig_figs = read_int(buf, read_bit, 6)
                read_bit += 6

            sig_bits = read_int(buf, read_bit, sig_figs)
            read_bit += sig_figs
            full_bits = sig_bits << (32 - sig_figs - leading_bits)
            final_value = full_bits ^ float_vals[-1]
            float_vals.append(final_value)
            
            state = "store"

    converted_floats = []
    for float_int in float_vals:
        byte_rep = struct.unpack('>f', float_int.to_bytes(4, 'big'))[0]
        converted_floats.append(byte_rep)
    
    return converted_floats
        
# Encode binary data to a base 64 string
binary_data = b'\xE8\x59\x00\x00\x1B\x42\xDA\xC3\xF7\x00'
# Use the codecs module to encode
base64_data = codecs.encode(binary_data, 'base64')
payload = base64.b64decode(base64_data)

print(decode(payload))